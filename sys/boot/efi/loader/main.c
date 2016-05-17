/*-
 * Copyright (c) 2008-2010 Rui Paulo
 * Copyright (c) 2006 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/boot.h>
#include <stand.h>
#include <string.h>
#include <setjmp.h>

#include <efi.h>
#include <efilib.h>

#include <uuid.h>

#include <bootstrap.h>
#include <smbios.h>

#ifdef EFI_ZFS_BOOT
#include <libzfs.h>
#endif

#include "loader_efi.h"

extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];

struct arch_switch archsw;	/* MI/MD interface boundary */

EFI_GUID acpi = ACPI_TABLE_GUID;
EFI_GUID acpi20 = ACPI_20_TABLE_GUID;
EFI_GUID devid = DEVICE_PATH_PROTOCOL;
EFI_GUID imgid = LOADED_IMAGE_PROTOCOL;
EFI_GUID mps = MPS_TABLE_GUID;
EFI_GUID netid = EFI_SIMPLE_NETWORK_PROTOCOL;
EFI_GUID smbios = SMBIOS_TABLE_GUID;
EFI_GUID dxe = DXE_SERVICES_TABLE_GUID;
EFI_GUID hoblist = HOB_LIST_TABLE_GUID;
EFI_GUID memtype = MEMORY_TYPE_INFORMATION_TABLE_GUID;
EFI_GUID debugimg = DEBUG_IMAGE_INFO_TABLE_GUID;
EFI_GUID fdtdtb = FDT_TABLE_GUID;
EFI_GUID inputid = SIMPLE_TEXT_INPUT_PROTOCOL;

#ifdef EFI_ZFS_BOOT
static void efi_zfs_probe(void);
#endif

/*
 * Need this because EFI uses UTF-16 unicode string constants, but we
 * use UTF-8. We can't use printf due to the possibility of \0 and we
 * don't support support wide characters either.
 */
static void
print_str16(const CHAR16 *str)
{
	int i;

	for (i = 0; str[i]; i++)
		printf("%c", (char)str[i]);
}

static void
cp16to8(const CHAR16 *src, char *dst, size_t len)
{
	size_t i;

	for (i = 0; i < len && src[i]; i++)
		dst[i] = (char)src[i];
}

static int
has_keyboard(void)
{
	EFI_STATUS status;
	EFI_DEVICE_PATH *path;
	EFI_HANDLE *hin, *hin_end, *walker;
	UINTN sz;
	int retval = 0;
	
	/*
	 * Find all the handles that support the SIMPLE_TEXT_INPUT_PROTOCOL and
	 * do the typical dance to get the right sized buffer.
	 */
	sz = 0;
	hin = NULL;
	status = BS->LocateHandle(ByProtocol, &inputid, 0, &sz, 0);
	if (status == EFI_BUFFER_TOO_SMALL) {
		hin = (EFI_HANDLE *)malloc(sz);
		status = BS->LocateHandle(ByProtocol, &inputid, 0, &sz,
		    hin);
		if (EFI_ERROR(status))
			free(hin);
	}
	if (EFI_ERROR(status))
		return retval;

	/*
	 * Look at each of the handles. If it supports the device path protocol,
	 * use it to get the device path for this handle. Then see if that
	 * device path matches either the USB device path for keyboards or the
	 * legacy device path for keyboards.
	 */
	hin_end = &hin[sz / sizeof(*hin)];
	for (walker = hin; walker < hin_end; walker++) {
		status = BS->HandleProtocol(*walker, &devid, (VOID **)&path);
		if (EFI_ERROR(status))
			continue;

		while (!IsDevicePathEnd(path)) {
			/*
			 * Check for the ACPI keyboard node. All PNP3xx nodes
			 * are keyboards of different flavors. Note: It is
			 * unclear of there's always a keyboard node when
			 * there's a keyboard controller, or if there's only one
			 * when a keyboard is detected at boot.
			 */
			if (DevicePathType(path) == ACPI_DEVICE_PATH &&
			    (DevicePathSubType(path) == ACPI_DP ||
				DevicePathSubType(path) == ACPI_EXTENDED_DP)) {
				ACPI_HID_DEVICE_PATH  *acpi;

				acpi = (ACPI_HID_DEVICE_PATH *)(void *)path;
				if ((EISA_ID_TO_NUM(acpi->HID) & 0xff00) == 0x300 &&
				    (acpi->HID & 0xffff) == PNP_EISA_ID_CONST) {
					retval = 1;
					goto out;
				}
			/*
			 * Check for USB keyboard node, if present. Unlike a
			 * PS/2 keyboard, these definitely only appear when
			 * connected to the system.
			 */
			} else if (DevicePathType(path) == MESSAGING_DEVICE_PATH &&
			    DevicePathSubType(path) == MSG_USB_CLASS_DP) {
				USB_CLASS_DEVICE_PATH *usb;
			       
				usb = (USB_CLASS_DEVICE_PATH *)(void *)path;
				if (usb->DeviceClass == 3 && /* HID */
				    usb->DeviceSubClass == 1 && /* Boot devices */
				    usb->DeviceProtocol == 1) { /* Boot keyboards */
					retval = 1;
					goto out;
				}
			}
			path = NextDevicePathNode(path);
		}
	}
out:
	free(hin);
	return retval;
}

EFI_STATUS
main(int argc, CHAR16 *argv[])
{
	char var[128];
	EFI_LOADED_IMAGE *img;
	EFI_GUID *guid;
	int i, j, vargood, unit, howto;
	struct devsw *dev;
	uint64_t pool_guid;
	UINTN k;
	int has_kbd;

	archsw.arch_autoload = efi_autoload;
	archsw.arch_getdev = efi_getdev;
	archsw.arch_copyin = efi_copyin;
	archsw.arch_copyout = efi_copyout;
	archsw.arch_readin = efi_readin;
#ifdef EFI_ZFS_BOOT
	/* Note this needs to be set before ZFS init. */
	archsw.arch_zfs_probe = efi_zfs_probe;
#endif

	has_kbd = has_keyboard();

	/*
	 * XXX Chicken-and-egg problem; we want to have console output
	 * early, but some console attributes may depend on reading from
	 * eg. the boot device, which we can't do yet.  We can use
	 * printf() etc. once this is done.
	 */
	cons_probe();

	/*
	 * Initialise the block cache. Set the upper limit.
	 */
	bcache_init(32768, 512);

	/*
	 * Parse the args to set the console settings, etc
	 * boot1.efi passes these in, if it can read /boot.config or /boot/config
	 * or iPXE may be setup to pass these in.
	 *
	 * Loop through the args, and for each one that contains an '=' that is
	 * not the first character, add it to the environment.  This allows
	 * loader and kernel env vars to be passed on the command line.  Convert
	 * args from UCS-2 to ASCII (16 to 8 bit) as they are copied.
	 */
	howto = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (j = 1; argv[i][j] != 0; j++) {
				int ch;

				ch = argv[i][j];
				switch (ch) {
				case 'a':
					howto |= RB_ASKNAME;
					break;
				case 'd':
					howto |= RB_KDB;
					break;
				case 'D':
					howto |= RB_MULTIPLE;
					break;
				case 'h':
					howto |= RB_SERIAL;
					break;
				case 'm':
					howto |= RB_MUTE;
					break;
				case 'p':
					howto |= RB_PAUSE;
					break;
				case 'P':
					if (!has_kbd)
						howto |= RB_SERIAL | RB_MULTIPLE;
					break;
				case 'r':
					howto |= RB_DFLTROOT;
					break;
				case 's':
					howto |= RB_SINGLE;
					break;
				case 'S':
					if (argv[i][j + 1] == 0) {
						if (i + 1 == argc) {
							setenv("comconsole_speed", "115200", 1);
						} else {
							cp16to8(&argv[i + 1][0], var,
							    sizeof(var));
							setenv("comconsole_speedspeed", var, 1);
						}
						i++;
						break;
					} else {
						cp16to8(&argv[i][j + 1], var,
						    sizeof(var));
						setenv("comconsole_speed", var, 1);
						break;
					}
				case 'v':
					howto |= RB_VERBOSE;
					break;
				}
			}
		} else {
			vargood = 0;
			for (j = 0; argv[i][j] != 0; j++) {
				if (j == sizeof(var)) {
					vargood = 0;
					break;
				}
				if (j > 0 && argv[i][j] == '=')
					vargood = 1;
				var[j] = (char)argv[i][j];
			}
			if (vargood) {
				var[j] = 0;
				putenv(var);
			}
		}
	}
	for (i = 0; howto_names[i].ev != NULL; i++)
		if (howto & howto_names[i].mask)
			setenv(howto_names[i].ev, "YES", 1);
	if (howto & RB_MULTIPLE) {
		if (howto & RB_SERIAL)
			setenv("console", "comconsole efi" , 1);
		else
			setenv("console", "efi comconsole" , 1);
	} else if (howto & RB_SERIAL) {
		setenv("console", "comconsole" , 1);
	}

	if (efi_copy_init()) {
		printf("failed to allocate staging area\n");
		return (EFI_BUFFER_TOO_SMALL);
	}

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	/* Get our loaded image protocol interface structure. */
	BS->HandleProtocol(IH, &imgid, (VOID**)&img);

	printf("Command line arguments:");
	for (i = 0; i < argc; i++) {
		printf(" ");
		print_str16(argv[i]);
	}
	printf("\n");

	printf("Image base: 0x%lx\n", (u_long)img->ImageBase);
	printf("EFI version: %d.%02d\n", ST->Hdr.Revision >> 16,
	    ST->Hdr.Revision & 0xffff);
	printf("EFI Firmware: %S (rev %d.%02d)\n", ST->FirmwareVendor,
	    ST->FirmwareRevision >> 16, ST->FirmwareRevision & 0xffff);

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);

	/*
	 * Disable the watchdog timer. By default the boot manager sets
	 * the timer to 5 minutes before invoking a boot option. If we
	 * want to return to the boot manager, we have to disable the
	 * watchdog timer and since we're an interactive program, we don't
	 * want to wait until the user types "quit". The timer may have
	 * fired by then. We don't care if this fails. It does not prevent
	 * normal functioning in any way...
	 */
	BS->SetWatchdogTimer(0, 0, 0, NULL);

	if (efi_handle_lookup(img->DeviceHandle, &dev, &unit, &pool_guid) != 0)
		return (EFI_NOT_FOUND);

	switch (dev->dv_type) {
#ifdef EFI_ZFS_BOOT
	case DEVT_ZFS: {
		struct zfs_devdesc currdev;

		currdev.d_dev = dev;
		currdev.d_unit = unit;
		currdev.d_type = currdev.d_dev->dv_type;
		currdev.d_opendata = NULL;
		currdev.pool_guid = pool_guid;
		currdev.root_guid = 0;
		env_setenv("currdev", EV_VOLATILE, efi_fmtdev(&currdev),
			   efi_setcurrdev, env_nounset);
		env_setenv("loaddev", EV_VOLATILE, efi_fmtdev(&currdev), env_noset,
			   env_nounset);
		init_zfs_bootenv(zfs_fmtdev(&currdev));
		break;
	}
#endif
	default: {
		struct devdesc currdev;

		currdev.d_dev = dev;
		currdev.d_unit = unit;
		currdev.d_opendata = NULL;
		currdev.d_type = currdev.d_dev->dv_type;
		env_setenv("currdev", EV_VOLATILE, efi_fmtdev(&currdev),
			   efi_setcurrdev, env_nounset);
		env_setenv("loaddev", EV_VOLATILE, efi_fmtdev(&currdev), env_noset,
			   env_nounset);
		break;
	}
	}

	snprintf(var, sizeof(var), "%d.%02d", ST->Hdr.Revision >> 16,
	    ST->Hdr.Revision & 0xffff);
	env_setenv("efi-version", EV_VOLATILE, var, env_noset, env_nounset);
	setenv("LINES", "24", 1);	/* optional */

	for (k = 0; k < ST->NumberOfTableEntries; k++) {
		guid = &ST->ConfigurationTable[k].VendorGuid;
		if (!memcmp(guid, &smbios, sizeof(EFI_GUID))) {
			smbios_detect(ST->ConfigurationTable[k].VendorTable);
			break;
		}
	}

	interact(NULL);			/* doesn't return */

	return (EFI_SUCCESS);		/* keep compiler happy */
}

/* XXX move to lib stand ? */
static int
wcscmp(CHAR16 *a, CHAR16 *b)
{

	while (*a && *b && *a == *b) {
		a++;
		b++;
	}
	return *a - *b;
}


COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{
	int i;

	for (i = 0; devsw[i] != NULL; ++i)
		if (devsw[i]->dv_cleanup != NULL)
			(devsw[i]->dv_cleanup)();

	RS->ResetSystem(EfiResetCold, EFI_SUCCESS, 23,
	    (CHAR16 *)"Reboot from the loader");

	/* NOTREACHED */
	return (CMD_ERROR);
}

COMMAND_SET(quit, "quit", "exit the loader", command_quit);

static int
command_quit(int argc, char *argv[])
{
	exit(0);
	return (CMD_OK);
}

COMMAND_SET(memmap, "memmap", "print memory map", command_memmap);

static int
command_memmap(int argc, char *argv[])
{
	UINTN sz;
	EFI_MEMORY_DESCRIPTOR *map, *p;
	UINTN key, dsz;
	UINT32 dver;
	EFI_STATUS status;
	int i, ndesc;
	static char *types[] = {
	    "Reserved",
	    "LoaderCode",
	    "LoaderData",
	    "BootServicesCode",
	    "BootServicesData",
	    "RuntimeServicesCode",
	    "RuntimeServicesData",
	    "ConventionalMemory",
	    "UnusableMemory",
	    "ACPIReclaimMemory",
	    "ACPIMemoryNVS",
	    "MemoryMappedIO",
	    "MemoryMappedIOPortSpace",
	    "PalCode"
	};

	sz = 0;
	status = BS->GetMemoryMap(&sz, 0, &key, &dsz, &dver);
	if (status != EFI_BUFFER_TOO_SMALL) {
		printf("Can't determine memory map size\n");
		return (CMD_ERROR);
	}
	map = malloc(sz);
	status = BS->GetMemoryMap(&sz, map, &key, &dsz, &dver);
	if (EFI_ERROR(status)) {
		printf("Can't read memory map\n");
		return (CMD_ERROR);
	}

	ndesc = sz / dsz;
	printf("%23s %12s %12s %8s %4s\n",
	    "Type", "Physical", "Virtual", "#Pages", "Attr");

	for (i = 0, p = map; i < ndesc;
	     i++, p = NextMemoryDescriptor(p, dsz)) {
		printf("%23s %012jx %012jx %08jx ", types[p->Type],
		   (uintmax_t)p->PhysicalStart, (uintmax_t)p->VirtualStart,
		   (uintmax_t)p->NumberOfPages);
		if (p->Attribute & EFI_MEMORY_UC)
			printf("UC ");
		if (p->Attribute & EFI_MEMORY_WC)
			printf("WC ");
		if (p->Attribute & EFI_MEMORY_WT)
			printf("WT ");
		if (p->Attribute & EFI_MEMORY_WB)
			printf("WB ");
		if (p->Attribute & EFI_MEMORY_UCE)
			printf("UCE ");
		if (p->Attribute & EFI_MEMORY_WP)
			printf("WP ");
		if (p->Attribute & EFI_MEMORY_RP)
			printf("RP ");
		if (p->Attribute & EFI_MEMORY_XP)
			printf("XP ");
		printf("\n");
	}

	return (CMD_OK);
}

COMMAND_SET(configuration, "configuration", "print configuration tables",
    command_configuration);

static const char *
guid_to_string(EFI_GUID *guid)
{
	static char buf[40];

	sprintf(buf, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	    guid->Data1, guid->Data2, guid->Data3, guid->Data4[0],
	    guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4],
	    guid->Data4[5], guid->Data4[6], guid->Data4[7]);
	return (buf);
}

static int
command_configuration(int argc, char *argv[])
{
	UINTN i;

	printf("NumberOfTableEntries=%lu\n",
		(unsigned long)ST->NumberOfTableEntries);
	for (i = 0; i < ST->NumberOfTableEntries; i++) {
		EFI_GUID *guid;

		printf("  ");
		guid = &ST->ConfigurationTable[i].VendorGuid;
		if (!memcmp(guid, &mps, sizeof(EFI_GUID)))
			printf("MPS Table");
		else if (!memcmp(guid, &acpi, sizeof(EFI_GUID)))
			printf("ACPI Table");
		else if (!memcmp(guid, &acpi20, sizeof(EFI_GUID)))
			printf("ACPI 2.0 Table");
		else if (!memcmp(guid, &smbios, sizeof(EFI_GUID)))
			printf("SMBIOS Table");
		else if (!memcmp(guid, &dxe, sizeof(EFI_GUID)))
			printf("DXE Table");
		else if (!memcmp(guid, &hoblist, sizeof(EFI_GUID)))
			printf("HOB List Table");
		else if (!memcmp(guid, &memtype, sizeof(EFI_GUID)))
			printf("Memory Type Information Table");
		else if (!memcmp(guid, &debugimg, sizeof(EFI_GUID)))
			printf("Debug Image Info Table");
		else if (!memcmp(guid, &fdtdtb, sizeof(EFI_GUID)))
			printf("FDT Table");
		else
			printf("Unknown Table (%s)", guid_to_string(guid));
		printf(" at %p\n", ST->ConfigurationTable[i].VendorTable);
	}

	return (CMD_OK);
}


COMMAND_SET(mode, "mode", "change or display EFI text modes", command_mode);

static int
command_mode(int argc, char *argv[])
{
	UINTN cols, rows;
	unsigned int mode;
	int i;
	char *cp;
	char rowenv[8];
	EFI_STATUS status;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout;
	extern void HO(void);

	conout = ST->ConOut;

	if (argc > 1) {
		mode = strtol(argv[1], &cp, 0);
		if (cp[0] != '\0') {
			printf("Invalid mode\n");
			return (CMD_ERROR);
		}
		status = conout->QueryMode(conout, mode, &cols, &rows);
		if (EFI_ERROR(status)) {
			printf("invalid mode %d\n", mode);
			return (CMD_ERROR);
		}
		status = conout->SetMode(conout, mode);
		if (EFI_ERROR(status)) {
			printf("couldn't set mode %d\n", mode);
			return (CMD_ERROR);
		}
		sprintf(rowenv, "%u", (unsigned)rows);
		setenv("LINES", rowenv, 1);
		HO();		/* set cursor */
		return (CMD_OK);
	}

	printf("Current mode: %d\n", conout->Mode->Mode);
	for (i = 0; i <= conout->Mode->MaxMode; i++) {
		status = conout->QueryMode(conout, i, &cols, &rows);
		if (EFI_ERROR(status))
			continue;
		printf("Mode %d: %u columns, %u rows\n", i, (unsigned)cols,
		    (unsigned)rows);
	}

	if (i != 0)
		printf("Select a mode with the command \"mode <number>\"\n");

	return (CMD_OK);
}


/* deprecated */
COMMAND_SET(nvram, "nvram", "get or set NVRAM variables", command_nvram);

static int
command_nvram(int argc, char *argv[])
{
	CHAR16 var[128];
	CHAR16 *data;
	EFI_STATUS status;
	EFI_GUID varguid = { 0,0,0,{0,0,0,0,0,0,0,0} };
	UINTN varsz, datasz, i;
	SIMPLE_TEXT_OUTPUT_INTERFACE *conout;

	conout = ST->ConOut;

	/* Initiate the search */
	status = RS->GetNextVariableName(&varsz, NULL, NULL);

	for (; status != EFI_NOT_FOUND; ) {
		status = RS->GetNextVariableName(&varsz, var, &varguid);
		//if (EFI_ERROR(status))
			//break;

		conout->OutputString(conout, var);
		printf("=");
		datasz = 0;
		status = RS->GetVariable(var, &varguid, NULL, &datasz, NULL);
		/* XXX: check status */
		data = malloc(datasz);
		status = RS->GetVariable(var, &varguid, NULL, &datasz, data);
		if (EFI_ERROR(status))
			printf("<error retrieving variable>");
		else {
			for (i = 0; i < datasz; i++) {
				if (isalnum(data[i]) || isspace(data[i]))
					printf("%c", data[i]);
				else
					printf("\\x%02x", data[i]);
			}
		}
		/* XXX */
		pager_output("\n");
		free(data);
	}

	return (CMD_OK);
}

#ifdef EFI_ZFS_BOOT
COMMAND_SET(lszfs, "lszfs", "list child datasets of a zfs dataset",
    command_lszfs);

static int
command_lszfs(int argc, char *argv[])
{
	int err;

	if (argc != 2) {
		command_errmsg = "wrong number of arguments";
		return (CMD_ERROR);
	}

	err = zfs_list(argv[1]);
	if (err != 0) {
		command_errmsg = strerror(err);
		return (CMD_ERROR);
	}
	return (CMD_OK);
}

COMMAND_SET(reloadbe, "reloadbe", "refresh the list of ZFS Boot Environments",
	    command_reloadbe);

static int
command_reloadbe(int argc, char *argv[])
{
	int err;
	char *root;

	if (argc > 2) {
		command_errmsg = "wrong number of arguments";
		return (CMD_ERROR);
	}

	if (argc == 2) {
		err = zfs_bootenv(argv[1]);
	} else {
		root = getenv("zfs_be_root");
		if (root == NULL) {
			return (CMD_OK);
		}
		err = zfs_bootenv(root);
	}

	if (err != 0) {
		command_errmsg = strerror(err);
		return (CMD_ERROR);
	}

	return (CMD_OK);
}
#endif

COMMAND_SET(efishow, "efi-show", "print some or all EFI variables", command_efi_printenv);

static int
efi_print_var(CHAR16 *varnamearg, EFI_GUID *matchguid, int lflag)
{
	UINTN		datasz;
	EFI_STATUS	status;
	UINT32		attr;
	CHAR16		*data;
	char		*str;
	uint32_t	uuid_status;

	datasz = 0;
	status = RS->GetVariable(varnamearg, matchguid, &attr,
	    &datasz, NULL);
	if (status != EFI_BUFFER_TOO_SMALL) {
		printf("Can't get the variable: error %#lx\n", status);
		return (CMD_ERROR);
	}
	data = malloc(datasz);
	status = RS->GetVariable(varnamearg, matchguid, &attr,
	    &datasz, data);
	if (status != EFI_SUCCESS) {
		printf("Can't get the variable: error %#lx\n", status);
		return (CMD_ERROR);
	}
	uuid_to_string((uuid_t *)matchguid, &str, &uuid_status);
	printf("%s %S=%S\n", str, varnamearg, data);
	free(str);
	free(data);
	return (CMD_OK);
}

static int
command_efi_printenv(int argc, char *argv[])
{
	/*
	 * efi-printenv [-a]
	 *	print all the env
	 * efi-printenv -u UUID
	 *	print all the env vars tagged with UUID
	 * efi-printenv -v var
	 *	search all the env vars and print the ones matching var
	 * eif-printenv -u UUID -v var
	 * eif-printenv UUID var
	 *	print all the env vars that match UUID and var
	 */
	/* XXX We assume EFI_GUID is the same as uuid_t */
	int		aflag = 0, gflag = 0, lflag = 0, vflag = 0;
	int		ch;
	unsigned	i;
	EFI_STATUS	status;
	EFI_GUID	varguid = { 0,0,0,{0,0,0,0,0,0,0,0} };
	EFI_GUID	matchguid = { 0,0,0,{0,0,0,0,0,0,0,0} };
	uint32_t	uuid_status;
	CHAR16		varname[128];
	CHAR16		varnamearg[128];
	UINTN		varsz;

	while ((ch = getopt(argc, argv, "ag:lv:")) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'g':
			gflag = 1;
			uuid_from_string(optarg, (uuid_t *)&matchguid,
			    &uuid_status);
			if (uuid_status != uuid_s_ok) {
				printf("uid %s could not be parsed\n", optarg);
				return (CMD_ERROR);
			}
			break;
		case 'l':
			lflag = 1;
			break;
		case 'v':
			vflag = 1;
			if (strlen(optarg) >= nitems(varnamearg)) {
				printf("Variable %s is longer than %zd characters\n",
				    optarg, nitems(varnamearg));
				return (CMD_ERROR);
			}
			for (i = 0; i < strlen(optarg); i++)
				varnamearg[i] = optarg[i];
			varnamearg[i] = 0;
		default:
			printf("Invalid argument %c\n", ch);
			return (CMD_ERROR);
		}
	}

	if (aflag && (gflag || vflag)) {
		printf("-a isn't compatible with -v or -u\n");
		return (CMD_ERROR);
	}

	if (aflag && optind < argc) {
		printf("-a doesn't take any args");
		return (CMD_ERROR);
	}

	if (optind == argc)
		aflag = 1;

	argc -= optind;
	argv += optind;

	if (vflag && gflag)
		return efi_print_var(varnamearg, &matchguid, lflag);

	if (argc == 2) {
		optarg = argv[0];
		if (strlen(optarg) >= nitems(varnamearg)) {
			printf("Variable %s is longer than %zd characters\n",
			    optarg, nitems(varnamearg));
			return (CMD_ERROR);
		}
		for (i = 0; i < strlen(optarg); i++)
			varnamearg[i] = optarg[i];
		varnamearg[i] = 0;
		optarg = argv[1];
		uuid_from_string(optarg, (uuid_t *)&matchguid,
		    &uuid_status);
		if (uuid_status != uuid_s_ok) {
			printf("uid %s could not be parsed\n", optarg);
			return (CMD_ERROR);
		}
		return efi_print_var(varnamearg, &matchguid, lflag);
	}

	if (argc != 0) {
		printf("Too many args\n");
		return (CMD_ERROR);
	}

	/*
	 * Initiate the search -- note the standard takes pain
	 * to specify the initial call must be a poiner to a NULL
	 * character.
	 */
	varsz = nitems(varname);
	varname[0] = 0;
	status = RS->GetNextVariableName(&varsz, varname, &varguid);
	while (status != EFI_NOT_FOUND) {
		status = RS->GetNextVariableName(&varsz, varname,
		    &varguid);
		if (aflag) {
			efi_print_var(varname, &varguid, lflag);
			continue;
		}
		if (vflag) {
			if (wcscmp(varnamearg, varname) == 0)
				efi_print_var(varname, &varguid, lflag);
		}
		if (gflag) {
			if (memcmp(&varguid, &matchguid, sizeof(varguid)) == 0)
				efi_print_var(varname, &varguid, lflag);
		}
	}
	
	return (CMD_OK);
}

COMMAND_SET(efiset, "efi-set", "set EFI variables", command_efi_set);

static int
command_efi_set(int argc, char *argv[])
{
	return (CMD_OK);
}

COMMAND_SET(efiunset, "efi-unset", "delete / unset EFI variables", command_efi_unset);

static int
command_efi_unset(int argc, char *argv[])
{
	return (CMD_OK);
}

#ifdef LOADER_FDT_SUPPORT
extern int command_fdt_internal(int argc, char *argv[]);

/*
 * Since proper fdt command handling function is defined in fdt_loader_cmd.c,
 * and declaring it as extern is in contradiction with COMMAND_SET() macro
 * (which uses static pointer), we're defining wrapper function, which
 * calls the proper fdt handling routine.
 */
static int
command_fdt(int argc, char *argv[])
{

	return (command_fdt_internal(argc, argv));
}

COMMAND_SET(fdt, "fdt", "flattened device tree handling", command_fdt);
#endif

#ifdef EFI_ZFS_BOOT
static void
efi_zfs_probe(void)
{
	EFI_HANDLE h;
	u_int unit;
	int i;
	char dname[SPECNAMELEN + 1];
	uint64_t guid;

	unit = 0;
	h = efi_find_handle(&efipart_dev, 0);
	for (i = 0; h != NULL; h = efi_find_handle(&efipart_dev, ++i)) {
		snprintf(dname, sizeof(dname), "%s%d:", efipart_dev.dv_name, i);
		if (zfs_probe_dev(dname, &guid) == 0)
			(void)efi_handle_update_dev(h, &zfs_dev, unit++, guid);
	}
}
#endif
