/*	$NetBSD: ohci_pci.c,v 1.5 1998/11/25 22:32:04 augustss Exp $	*/
/*	FreeBSD $Id$ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * USB Open Host Controller driver.
 *
 * OHCI spec: http://www.intel.com/design/usb/ohci11d.pdf
 * USB spec: http://www.teleport.com/cgi-bin/mailmerge.cgi/~usb/cgiform.tpl
 */

#include <dev/usb/usb_port.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#if defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#if defined(__NetBSD__)
#include <machine/bus.h>
#include <dev/pci/pcivar.h>
#elif defined(__FreeBSD__)
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#define PCI_CLASS_SERIALBUS                     0x0c000000
#define PCI_SUBCLASS_COMMUNICATIONS_SERIAL      0x00000000
#define PCI_SUBCLASS_SERIALBUS_FIREWIRE         0x00000000
#define PCI_SUBCLASS_SERIALBUS_ACCESS           0x00010000
#define PCI_SUBCLASS_SERIALBUS_SSA              0x00020000
#define PCI_SUBCLASS_SERIALBUS_USB              0x00030000
#define PCI_SUBCLASS_SERIALBUS_FIBER            0x00040000
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

#if defined(__NetBSD__)
int	ohci_pci_match __P((struct device *, struct cfdata *, void *));
void	ohci_pci_attach __P((struct device *, struct device *, void *));

struct cfattach ohci_pci_ca = {
	sizeof(struct ohci_softc), ohci_pci_match, ohci_pci_attach
};
#elif defined(__FreeBSD__)

#define PCI_INTERFACE(d)        (((d) >> 8) & 0xff)
#define PCI_SUBCLASS(d)         ((d) & PCI_SUBCLASS_MASK)
#define PCI_CLASS(d)            ((d) & PCI_CLASS_MASK)

#define PCI_VENDOR(d)           ((d) & 0xffff)
#define PCI_DEVICE(d)           (((d) >> 8) & 0xffff)

#define PCI_OHCI_BASE_REG	0x10

static const char *ohci_pci_probe              __P((pcici_t, pcidi_t));
static void ohci_pci_attach              __P((pcici_t, int));

u_long ohci_count = 0;           /* global counter for nr. of devices found */

static struct pci_device ohci_pci_device = {
        "ohci",
	ohci_pci_probe,
	ohci_pci_attach,
	&ohci_count,
	NULL
};

DATA_SET(pcidevice_set, ohci_pci_device);
#endif

#if defined(__NetBSD__)
int
ohci_pci_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_OHCI)
		return 1;
 
	return 0;
}
#elif defined(__FreeBSD__)
static const char *
ohci_pci_probe(pcici_t config_id, pcidi_t device_id)
{
        u_int32_t class;

	class = pci_conf_read(config_id, PCI_CLASS_REG);
	if (   (PCI_CLASS(class)     == PCI_CLASS_SERIALBUS)
	    && (PCI_SUBCLASS(class)  == PCI_SUBCLASS_SERIALBUS_USB)
	    && (PCI_INTERFACE(class) == PCI_INTERFACE_OHCI))
		return("OHCI Host Controller");

	return NULL;	/* dunno */
}
#endif

#if defined(__NetBSD__)
void
ohci_pci_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct ohci_softc *sc = (struct ohci_softc *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t csr;
	char devinfo[256];
	usbd_status r;
	char *vendor;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_CBMEM, PCI_MAPREG_TYPE_MEM, 0,
			   &sc->iot, &sc->ioh, NULL, NULL)) {
		printf("%s: can't map mem space\n", sc->sc_bus.bdev.dv_xname);
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;

	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_bus.bdev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_USB, ohci_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_bus.bdev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_bus.bdev.dv_xname, intrstr);

	/* Figure out vendor for root hub descriptor. */
	vendor = pci_findvendor(pa->pa_id);
	if (vendor)
		strncpy(sc->sc_vendor, vendor, sizeof(sc->sc_vendor));
	else
		sprintf(sc->sc_vendor, "vendor 0x%04x", PCI_VENDOR(pa->pa_id));
	
	r = ohci_init(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", sc->sc_bus.bdev.dv_xname,
		       r);
		return;
	}

	/* Attach usb device. */
	config_found((void *)sc, &sc->sc_bus, usbctlprint);
}
#elif defined(__FreeBSD__)

static void
ohci_pci_attach(pcici_t config_id, int unit)
{
	int id;
	usbd_status r;
	ohci_softc_t *sc = NULL;
	int rev;

	sc = malloc(sizeof(ohci_softc_t), M_DEVBUF, M_NOWAIT);
	/* Do not free it below, intr might use the sc */
	if ( sc == NULL ) {
		printf("usb%d: could not allocate memory", unit);
		return;
	}
	memset(sc, 0, sizeof(ohci_softc_t));

	sc->sc_iobase = pci_conf_read(config_id,PCI_OHCI_BASE_REG) & 0xfffff000;
	sc->unit      = unit;

	if ( !pci_map_int(config_id, (pci_inthand_t *)ohci_intr,
			  (void *) sc, &bio_imask)) {
		printf("usb%d: Unable to map irq\n", unit);
		return;                    
	}

#ifndef USBVERBOSE
	if (bootverbose)
#endif
	{
		/* XXX is this correct? Does the correct version show up? */
		rev = inw(sc->sc_iobase+OHCI_REVISION);
		printf("usb%d: OHCI version %d%d, interrupting at %d\n", unit,
			OHCI_REV_HI(rev), OHCI_REV_LO(rev),
			(int)pci_conf_read(config_id,PCI_INTERRUPT_REG) & 0xff);
	}

	/* Figure out vendor for root hub descriptor. */
	id = pci_conf_read(config_id, PCI_ID_REG);
	if (PCI_VENDOR(id) == 0x8086)
		sprintf(sc->sc_vendor, "Intel");
	else
		sprintf(sc->sc_vendor, "Vendor 0x%04x", PCI_VENDOR(id));

	/* We add a child to the root bus. After PCI configuration
	 * has completed the root bus will start to probe and
	 * attach all the devices attached to it, including our new
	 * kid.
	 *
	 * FIXME Sometime in the future the UHCI controller itself will
	 * become a kid of PCI device and this device add will no longer
	 * be necessary.
	 *
	 * See README for an elaborate description of the bus
	 * structure in spe.
	 */
	sc->sc_bus.bdev = device_add_child(root_bus, "usb", unit, sc);
	if (!sc->sc_bus.bdev) {
		DEVICE_ERROR(sc->sc_bus.bdev,
			("unable to add USB device to root bus\n"));
		return;
	}

	r = ohci_init(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("usb%d: init failed, error=%d\n", unit, r);
		device_delete_child(root_bus, sc->sc_bus.bdev);
		return;
	}

	device_set_desc(sc->sc_bus.bdev, "OHCI Host Controller");

	return;
}
#endif
