/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)autoconf.c	7.20 (Berkeley) %G%
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time and initializes the uba and mba
 * device tables and the memory controller monitoring.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include "mba.h"
#include "uba.h"
#include "kra.h"		/* XXX wrong file */
#include "bi.h"

#include "sys/param.h"
#include "sys/systm.h"
#include "sys/map.h"
#include "sys/buf.h"
#include "sys/dkstat.h"
#include "sys/vm.h"
#include "sys/malloc.h"
#include "sys/conf.h"
#include "sys/dmap.h"
#include "sys/reboot.h"
#ifdef SECSIZE
#include "file.h"
#include "ioctl.h"
#include "disklabel.h"
#endif SECSIZE

#include "../include/pte.h"
#include "../include/cpu.h"
#include "mem.h"
#include "../include/mtpr.h"
#include "nexus.h"
#include "scb.h"
#include "ioa.h"
#include "../bi/bireg.h"
#include "../mba/mbareg.h"
#include "../mba/mbavar.h"
#include "../uba/ubareg.h"
#include "../uba/ubavar.h"

/*
 * The following several variables are related to
 * the configuration process, and are used in initializing
 * the machine.
 */
int	cold;		/* if 1, still working on cold-start */
int	dkn;		/* number of iostat dk numbers assigned so far */
int	cpuspeed = 1;	/* relative cpu speed */

/*
 * Addresses of the (locore) routines which bootstrap us from
 * hardware traps to C code.  Filled into the system control block
 * as necessary.
 *
 * RIDICULOUS!  CONFIG SHOULD GENERATE AN ioconf.h FOR US, with
 * mba glue also in `glue.s'.  (Unibus adapter glue is special, though.)
 */
#if NMBA > 0
int	(*mbaintv[4])() =	{ Xmba0int, Xmba1int, Xmba2int, Xmba3int };
#if NMBA > 4
	Need to expand the table for more than 4 massbus adaptors
#endif
#endif
#if defined(VAX780) || defined(VAX8600)
int	(*ubaintv[])() =
{
	Xua0int, Xua1int, Xua2int, Xua3int,
#if NUBA > 4
	Xua4int, Xua5int, Xua6int, Xua7int,
#endif
#if NUBA > 8
	Need to expand the table for more than 8 unibus adaptors
#endif
};
#endif
#if NKDB > 0
/* kdb50 driver does not appear in udminit[] (not without csr!) */
int	Xkdbintr0();		/* generated by autoconf */
int	(*kdbintv[])() = { Xkdbintr0 };
#if NKDB > 1
	Need to expand the table for more than 1 KDB adapter
#endif
#endif

/*
 * This allocates the space for the per-uba information,
 * such as buffered data path usage.
 */
struct	uba_hd uba_hd[NUBA];

/*
 * Determine mass storage and memory configuration for a machine.
 * Get cpu type, and then switch out to machine specific procedures
 * which will probe adaptors to see what is out there.
 */
configure()
{
	union cpusid cpusid;
	register struct percpu *ocp;
	register struct pte *ip;

	cpusid.cpusid = mfpr(SID);
	switch (cpusid.cpuany.cp_type) {
#if VAX8600
	case VAX_8600:
		printf("VAX 8600, serial# %d(%d), hardware ECO level %d(%d)\n",
			cpusid.cpu8600.cp_sno, cpusid.cpu8600.cp_plant,
			cpusid.cpu8600.cp_eco >> 4, cpusid.cpu8600.cp_eco);
		break;
#endif
#if VAX8200
	case VAX_8200:
		printf("\
VAX 82%c0, hardware rev %d, ucode patch rev %d, sec patch %d, ucode rev %d\n",
			cpusid.cpu8200.cp_5 ? '5' : '0',
			cpusid.cpu8200.cp_hrev, cpusid.cpu8200.cp_patch,
			cpusid.cpu8200.cp_secp, cpusid.cpu8200.cp_urev);
		mastercpu = mfpr(BINID);
		break;
#endif
#if VAX780
	case VAX_780:
		printf("\
VAX 11/78%c, serial# %d(%d), hardware ECO level %d(%d)\n",
			cpusid.cpu780.cp_5 ? '5' : '0',
			cpusid.cpu780.cp_sno, cpusid.cpu780.cp_plant,
			cpusid.cpu780.cp_eco >> 4, cpusid.cpu780.cp_eco);
		break;
#endif
#if VAX750
	case VAX_750:
		printf("VAX 11/750, hardware rev %d, ucode rev %d\n",
			cpusid.cpu750.cp_hrev, cpusid.cpu750.cp_urev);
		break;
#endif
#if VAX730
	case VAX_730:
		printf("VAX 11/730, ucode rev %d\n", cpusid.cpu730.cp_urev);
		break;
#endif
#if VAX630
	case VAX_630:
		printf("MicroVAX-II\n");
		break;
#endif
#if VAX650
	case VAX_650:
		printf("MicroVAX 3000, ucode rev %d\n", cpusid.cpu650.cp_urev);
		break;
#endif
	}
	for (ocp = percpu; ocp->pc_cputype; ocp++)
		if (ocp->pc_cputype == cpusid.cpuany.cp_type) {
			cpuspeed = ocp->pc_cpuspeed;
			cpuops = ocp->pc_ops;
			if (cpuops->cpu_init != NULL)
				(*cpuops->cpu_init)();
			probeio(ocp);
			/*
			 * Write protect the scb and UNIBUS interrupt vectors.
			 * It is strange that this code is here, but this is
			 * as soon as we are done mucking with it, and the
			 * write-enable was done in assembly language
			 * to which we will never return.
			 */
			for (ip = kvtopte(scb); ip < kvtopte(eUNIvec); ip++) {
				*(int *)ip &= ~PG_PROT;
				*(int *)ip |= PG_KR;
			}
			mtpr(TBIA, 0);
#if GENERIC
			if ((boothowto & RB_ASKNAME) == 0)
				setroot();
			setconf();
#else
			setroot();
#endif
			/*
			 * Configure swap area and related system
			 * parameter based on device(s) used.
			 */
			swapconf();
			cold = 0;
			memenable();
			return;
		}
	printf("cpu type %d not configured\n", cpusid.cpuany.cp_type);
	asm("halt");
}

#if VAX8600 || VAX780 || VAX750 || VAX730
int	nexnum;		/* current nexus number */
int	nsbi;		/* current sbi number */
#endif
#if VAX8200
int	numkdb;		/* current ``kdb'' number */
int	bi_nodes;	/* XXX remembers found bi nodes */
#endif

/*
 * Probe the main IO bus(es).
 * The percpu structure gives us a handle on the addresses and/or types.
 */
probeio(pcpu)
	register struct percpu *pcpu;
{
	register struct iobus *iob;
	int ioanum;

	ioanum = 0;
	for (iob = pcpu->pc_io; ioanum < pcpu->pc_nioa; ioanum++, iob++) {

		switch (iob->io_type) {

#if VAX630 || VAX650
		case IO_QBUS:
			probeqbus((struct qbus *)iob->io_details);
			break;
#endif

#if VAX780 || VAX750 || VAX730
		case IO_SBI780:
		case IO_CMI750:
		case IO_XXX730:
			probenexi((struct nexusconnect *)iob->io_details);
			break;
#endif

#if VAX8600
		case IO_ABUS:
			probe_Abus(ioanum, iob);
			break;
#endif

#if VAX8200
		case IO_BI:
			probe_bi((struct bibus *)iob->io_details);
			break;
#endif

		default:
			if (iob->io_addr) {
			    printf(
		"IO adaptor %d, type %d, at address 0x%x is unsupported\n",
				ioanum, iob->io_type, iob->io_addr);
			} else
			    printf("IO adaptor %d, type %d, is unsupported\n",
				ioanum, iob->io_type);
			break;
		}
	}
}

#if VAX8600
probe_Abus(ioanum, iob)
	register struct iobus *iob;
{
	register struct ioa *ioap;
	union ioacsr ioacsr;
	int type;
	struct sbia_regs *sbiaregs;
#ifdef notyet
	int sbi1fail(), sbi1alert(), sbi1fault(), sbi1err();
#endif

	ioap = &ioa[ioanum];
	ioaccess(iob->io_addr, Ioamap[ioanum], iob->io_size);
	if (badaddr((caddr_t)ioap, 4))
		return;
	ioacsr.ioa_csr = ioap->ioacsr.ioa_csr;
	type = ioacsr.ioa_type & IOA_TYPMSK;

	switch (type) {

	case IOA_SBIA:
		printf("SBIA%d at IO adaptor %d address 0x%x\n",
		    nsbi, ioanum, iob->io_addr);
#ifdef notyet
		/* I AM NOT SURE THESE ARE IN THE SAME PLACES */
		if (nscb == 1) {
			scb[1].scb_sbifail = scbentry(sbi1fail, SCB_ISTACK);
			/* maybe not sbifail, maybe scb1.scb_cmrd */
			/* but how can I find out without a broken SBIA1? */
			scb[1].scb_sbialert = scbentry(sbi1alert, SCB_ISTACK);
			scb[1].scb_sbifault = scbentry(sbi1fault, SCB_ISTACK);
			scb[1].scb_sbierr = scbentry(sbi1err, SCB_ISTACK);
		}
#endif
		probenexi((struct nexusconnect *)iob->io_details);
		nsbi++;
		sbiaregs = (struct sbia_regs *)ioap;
		sbiaregs->sbi_errsum = -1;
		sbiaregs->sbi_error = 0x1000;
		sbiaregs->sbi_fltsts = 0xc0000;
		break;

	default:
		printf("IOA%d at address 0x%x is unsupported (type = 0x%x)\n",
		    ioanum, iob->io_addr, ioacsr.ioa_type);
		break;
	}
}
#endif

#if VAX8600 || VAX780 || VAX750 || VAX730
/*
 * Probe nexus space, finding the interconnects
 * and setting up and probing mba's and uba's for devices.
 */
probenexi(pnc)
	register struct nexusconnect *pnc;
{
	register struct nexus *nxv;
	struct nexus *nxp = pnc->psb_nexbase;
	union nexcsr nexcsr;
	int i;

	ioaccess((caddr_t)nxp, Nexmap[nsbi * NNEXSBI],
	    pnc->psb_nnexus * sizeof(struct nexus));
	nxv = &nexus[nsbi * NNEXSBI];
	for (nexnum = 0; nexnum < pnc->psb_nnexus; nexnum++, nxp++, nxv++) {
		if (badaddr((caddr_t)nxv, 4))
			continue;
		if (pnc->psb_nextype && pnc->psb_nextype[nexnum] != NEX_ANY)
			nexcsr.nex_csr = pnc->psb_nextype[nexnum];
		else
			nexcsr = nxv->nexcsr;
		if (nexcsr.nex_csr&NEX_APD)
			continue;
		switch (nexcsr.nex_type) {

		case NEX_MBA:
			printf("mba%d at tr%d\n", nummba, nexnum);
			if (nummba >= NMBA) {
				printf("%d mba's", ++nummba);
				goto unconfig;
			}
#if NMBA > 0
			mbafind(nxv, nxp);
			nummba++;
#endif
			break;

		case NEX_UBA0:
		case NEX_UBA1:
		case NEX_UBA2:
		case NEX_UBA3:
			printf("uba%d at tr%d\n", numuba, nexnum);
			if (numuba >= NUBA) {
				printf("%d uba's", ++numuba);
				goto unconfig;
			}
#if NUBA > 0
#if VAX750
			if (numuba >= 2 && cpu == VAX_750) {
				printf("More than 2 UBA's");
				goto unsupp;
			}
#endif
#if defined(VAX780) || defined(VAX8600)
			if (cpu == VAX_780 || cpu == VAX_8600)
				setscbnex(ubaintv[numuba]);
#endif
			i = nexcsr.nex_type - NEX_UBA0;
			probeuba((struct uba_regs *)nxv, (struct uba_regs *)nxp,
			    pnc->psb_umaddr[i]);
#endif /* NUBA */
			break;

		case NEX_DR32:
		/* there can be more than one... are there other codes??? */
			printf("dr32");
			goto unsupp;

		case NEX_MEM4:
		case NEX_MEM4I:
		case NEX_MEM16:
		case NEX_MEM16I:
			printf("mcr%d at tr%d\n", nmcr, nexnum);
			if (nmcr >= MAXNMCR) {
				printf("%d mcr's", ++nmcr);
				goto unconfig;
			}
			switch (cpu) {
#if VAX780
			case VAX_780:
				/* only ka780 code looks at type */
				mcrtype[nmcr] = M780C;
				break;
#endif
			default:
				break;
			}
			mcraddr[nmcr++] = (caddr_t)nxv;
			break;

#if VAX780
		case NEX_MEM64I:
		case NEX_MEM64L:
		case NEX_MEM64LI:
		case NEX_MEM256I:
		case NEX_MEM256L:
		case NEX_MEM256LI:
			printf("mcr%d (el) at tr%d\n", nmcr, nexnum);
			if (nmcr >= MAXNMCR) {
				printf("%d mcr's", ++nmcr);
				goto unconfig;
			}
			mcrtype[nmcr] = M780EL;
			mcraddr[nmcr++] = (caddr_t)nxv;
			if (nexcsr.nex_type != NEX_MEM64I && 
			    nexcsr.nex_type != NEX_MEM256I)
				break;
			/* fall into ... */

		case NEX_MEM64U:
		case NEX_MEM64UI:
		case NEX_MEM256U:
		case NEX_MEM256UI:
			printf("mcr%d (eu) at tr%d\n", nmcr, nexnum);
			if (nmcr >= MAXNMCR) {
				printf("%d mcr's", ++nmcr);
				goto unconfig;
			}
			mcrtype[nmcr] = M780EU;
			mcraddr[nmcr++] = (caddr_t)nxv;
			break;
#endif

		case NEX_MPM0:
		case NEX_MPM1:
		case NEX_MPM2:
		case NEX_MPM3:
			printf("mpm");
			goto unsupp;

		case NEX_CI:
			printf("ci");
			goto unsupp;

		default:
			printf("nexus type %x", nexcsr.nex_type);
unsupp:
			printf(" unsupported (at tr %d)\n", nexnum);
			continue;
unconfig:
			printf(" not configured\n");
			continue;
		}
	}
	if (nummba > NMBA)
		nummba = NMBA;
	if (numuba > NUBA)
		numuba = NUBA;
	if (nmcr > MAXNMCR)
		nmcr = MAXNMCR;
}

setscbnex(fn)
	int (*fn)();
{
	register struct scb *scbp = &scb[nsbi];

	scbp->scb_ipl14[nexnum] = scbp->scb_ipl15[nexnum] =
	    scbp->scb_ipl16[nexnum] = scbp->scb_ipl17[nexnum] =
		scbentry(fn, SCB_ISTACK);
}
#endif

#if NBI > 0
/*
 * Probe BI node space.
 *
 * THIS DEPENDS ON BI SPACE == NEXUS SPACE
 * THIS WILL NOT WORK FOR MULTIPLE BIs
 */
probe_bi(p)
	register struct bibus *p;
{
	register struct bi_node *biv, *bip;
	register int node;
	short dtype;

	/* must ignore BI errors while configuring */
	bip = p->pbi_base;
	ioaccess((caddr_t)bip, Nexmap[0], sizeof(*bip) * NNODEBI);/* XXX */
	printf("vaxbi0 at address 0x%x\n", bip);
	biv = (struct bi_node *) &nexus[0];	/* XXX */
	for (node = 0; node < NNODEBI; node++, bip++, biv++) {
		if (badaddr((caddr_t)biv, 4))
			continue;
		bi_nodes |= 1 << node;		/* XXX */
		dtype = biv->biic.bi_dtype;
		/* clear bus errors */
		biv->biic.bi_ber = ~(BIBER_MBZ|BIBER_NMR|BIBER_UPEN);
		switch (dtype) {

		case BIDT_KA820: {
			/* is this right?? */
			int cp5 = biv->biic.bi_revs & 0x8000 ? '5' : '0';

			if (node != mastercpu) {
				printf("slave ka82%c cpu", cp5);
				goto unsupp;
			}
			printf("ka82%c cpu at node %x\n", cp5, node);
			biv->biic.bi_intrdes = 1 << mastercpu;
			biv->biic.bi_csr |= BICSR_SEIE | BICSR_HEIE;
			break;
		}

		case BIDT_DWBUA:
			if (numuba >= NUBA || /*XXX*/numuba > 2) {
				printf("%d uba's", ++numuba);
				goto unconfig;
			}
#if NUBA > 0
			printf("uba%d at node %x\n", numuba, node);

			/*
			 * Run a self test reset to drop any `old' errors,
			 * so that they cannot cause a BI bus error.
			 */
			(void) bi_selftest(&biv->biic);

			/*
			 * Enable interrupts.  DWBUAs must have
			 * high priority.
			 */
			biv->biic.bi_intrdes = 1 << mastercpu;
			biv->biic.bi_csr = (biv->biic.bi_csr&~BICSR_ARB_MASK) |
				BICSR_ARB_HIGH;
			probeuba((struct uba_regs *)biv, (struct uba_regs *)bip,
				(caddr_t)UMEM8200(node));
#endif /* NUBA */
			break;

		case BIDT_MS820:
			printf("mcr%d at node %x\n", nmcr, node);
			if (nmcr >= MAXNMCR) {
				printf("%d mcr's", ++nmcr);
				goto unconfig;
			}
			mcraddr[nmcr++] = (caddr_t)biv;
			biv->biic.bi_intrdes = 1 << mastercpu;
			biv->biic.bi_csr |= BICSR_SEIE | BICSR_HEIE;
			break;

		case BIDT_KDB50:
			if (numkdb >= NKDB) {
				printf("%d kdb's", ++numkdb);
				goto unconfig;
			}
#if NKDB > 0
			printf("kdb%d at node %x\n", numkdb, node);
			kdbconfig(numkdb, (struct biiregs *)biv,
				(struct biiregs *)bip,
				(int)&scb[0].scb_ipl15[node] - (int)&scb[0]);
			scb[0].scb_ipl15[node] =
				scbentry(kdbintv[numkdb], SCB_ISTACK);
			kdbfind(numkdb);
#endif
			numkdb++;
			break;

		case BIDT_DEBNA:
		case BIDT_DEBNK:
			printf("debna/debnk ethernet");
			goto unsupp;

		default:
			printf("node type 0x%x ", dtype);
unsupp:
			printf(" unsupported (at node %x)\n", node);
			break;
unconfig:
			printf(" not configured (at node %x)\n", node);
			continue;
		}
#ifdef DO_EINTRCSR
		biv->biic.bi_eintrcsr = BIEIC_IPL17 |
			(int)&scb[0].scb_bierr - (int)&scb[0];
		/* but bi reset will need to restore this */
#endif
	}
	if (numuba > NUBA)
		numuba = NUBA;
	if (numkdb > NKDB)
		numkdb = NKDB;
	if (nmcr > MAXNMCR)
		nmcr = MAXNMCR;
}

#if NKDB > 0
/*
 * Find drives attached to a particular KDB50.
 */
kdbfind(kdbnum)
	int kdbnum;
{
	extern struct uba_driver kdbdriver;
	register struct uba_device *ui;
	register struct uba_driver *udp = &kdbdriver;
	int t;

	for (ui = ubdinit; ui->ui_driver; ui++) {
		/* ui->ui_ubanum is trash */
		if (ui->ui_driver != udp || ui->ui_alive ||
		    ui->ui_ctlr != kdbnum && ui->ui_ctlr != '?')
			continue;
		t = ui->ui_ctlr;
		ui->ui_ctlr = kdbnum;
		if ((*udp->ud_slave)(ui) == 0) {
			ui->ui_ctlr = t;
			continue;
		}
		ui->ui_alive = 1;
		ui->ui_ubanum = -1;

		/* make these invalid so we can see if someone uses them */
		/* might as well make each one different too */
		ui->ui_hd = (struct uba_hd *)0xc0000010;
		ui->ui_addr = (caddr_t)0xc0000014;
		ui->ui_physaddr = (caddr_t)0xc0000018;
		ui->ui_mi = (struct uba_ctlr *)0xc000001c;

		if (ui->ui_dk && dkn < DK_NDRIVE)
			ui->ui_dk = dkn++;
		else
			ui->ui_dk = -1;
		/* ui_type comes from driver */
		udp->ud_dinfo[ui->ui_unit] = ui;
		printf("%s%d at %s%d slave %d\n",
		    udp->ud_dname, ui->ui_unit,
		    udp->ud_mname, ui->ui_ctlr, ui->ui_slave);
		(*udp->ud_attach)(ui);
	}
}
#endif /* NKDB > 0 */
#endif /* NBI > 0 */

#if NMBA > 0
struct	mba_device *mbaconfig();
/*
 * Find devices attached to a particular mba
 * and look for each device found in the massbus
 * initialization tables.
 */
mbafind(nxv, nxp)
	struct nexus *nxv, *nxp;
{
	register struct mba_regs *mdp;
	register struct mba_drv *mbd;
	register struct mba_device *mi;
	register struct mba_slave *ms;
	int dn, dt, sn;
	struct mba_device fnd;

	mdp = (struct mba_regs *)nxv;
	mba_hd[nummba].mh_mba = mdp;
	mba_hd[nummba].mh_physmba = (struct mba_regs *)nxp;
	setscbnex(mbaintv[nummba]);
	mdp->mba_cr = MBCR_INIT;
	mdp->mba_cr = MBCR_IE;
	fnd.mi_mba = mdp;
	fnd.mi_mbanum = nummba;
	for (mbd = mdp->mba_drv, dn = 0; mbd < &mdp->mba_drv[8]; mbd++, dn++) {
		if ((mbd->mbd_ds&MBDS_DPR) == 0)
			continue;
		mdp->mba_sr |= MBSR_NED;		/* si kludge */
		dt = mbd->mbd_dt & 0xffff;
		if (dt == 0)
			continue;
		if (mdp->mba_sr&MBSR_NED)
			continue;			/* si kludge */
		if (dt == MBDT_MOH)
			continue;
		fnd.mi_drive = dn;
#define	qeq(a, b)	( a == b || a == '?' )
		if ((mi = mbaconfig(&fnd, dt)) && (dt & MBDT_TAP))
		    for (sn = 0; sn < 8; sn++) {
			mbd->mbd_tc = sn;
		        for (ms = mbsinit; ms->ms_driver; ms++)
			    if (ms->ms_driver == mi->mi_driver &&
				ms->ms_alive == 0 && 
				qeq(ms->ms_ctlr, mi->mi_unit) &&
				qeq(ms->ms_slave, sn) &&
				(*ms->ms_driver->md_slave)(mi, ms, sn)) {
					printf("%s%d at %s%d slave %d\n"
					    , ms->ms_driver->md_sname
					    , ms->ms_unit
					    , mi->mi_driver->md_dname
					    , mi->mi_unit
					    , sn
					);
					ms->ms_alive = 1;
					ms->ms_ctlr = mi->mi_unit;
					ms->ms_slave = sn;
					break;
				}
		    }
	}
}

/*
 * Have found a massbus device;
 * see if it is in the configuration table.
 * If so, fill in its data.
 */
struct mba_device *
mbaconfig(ni, type)
	register struct mba_device *ni;
	register int type;
{
	register struct mba_device *mi;
	register short *tp;
	register struct mba_hd *mh;

	for (mi = mbdinit; mi->mi_driver; mi++) {
		if (mi->mi_alive)
			continue;
		tp = mi->mi_driver->md_type;
		for (mi->mi_type = 0; *tp; tp++, mi->mi_type++)
			if (*tp == (type&MBDT_TYPE))
				goto found;
		continue;
found:
#define	match(fld)	(ni->fld == mi->fld || mi->fld == '?')
		if (!match(mi_drive) || !match(mi_mbanum))
			continue;
		printf("%s%d at mba%d drive %d",
		    mi->mi_driver->md_dname, mi->mi_unit,
		    ni->mi_mbanum, ni->mi_drive);
		mi->mi_alive = 1;
		mh = &mba_hd[ni->mi_mbanum];
		mi->mi_hd = mh;
		mh->mh_mbip[ni->mi_drive] = mi;
		mh->mh_ndrive++;
		mi->mi_mba = ni->mi_mba;
		mi->mi_drv = &mi->mi_mba->mba_drv[ni->mi_drive];
		mi->mi_mbanum = ni->mi_mbanum;
		mi->mi_drive = ni->mi_drive;
		/*
		 * If drive has never been seen before,
		 * give it a dkn for statistics.
		 */
		if (mi->mi_driver->md_info[mi->mi_unit] == 0) {
			mi->mi_driver->md_info[mi->mi_unit] = mi;
			if (mi->mi_dk && dkn < DK_NDRIVE)
				mi->mi_dk = dkn++;
			else
				mi->mi_dk = -1;
		}
		(*mi->mi_driver->md_attach)(mi);
		printf("\n");
		return (mi);
	}
	return (0);
}
#endif

/*
 * Fixctlrmask fixes the masks of the driver ctlr routines
 * which otherwise save r10 and r11 where the interrupt and br
 * level are passed through.
 */
fixctlrmask()
{
	register struct uba_ctlr *um;
	register struct uba_device *ui;
	register struct uba_driver *ud;
#define	phys(a,b) ((b)(((int)(a))&0x7fffffff))

	for (um = ubminit; ud = phys(um->um_driver, struct uba_driver *); um++)
		*phys(ud->ud_probe, short *) &= ~0xc00;
	for (ui = ubdinit; ud = phys(ui->ui_driver, struct uba_driver *); ui++)
		*phys(ud->ud_probe, short *) &= ~0xc00;
}

#ifdef QBA
/*
 * Configure a Q-bus.
 */
probeqbus(qb)
	struct qbus *qb;
{
	register struct uba_hd *uhp = &uba_hd[numuba];

	ioaccess((caddr_t)qb->qb_map, Nexmap[0],
		qb->qb_memsize * sizeof (struct pte));
	uhp->uh_type = qb->qb_type;
	uhp->uh_uba = (struct uba_regs *)0xc0000000;   /* no uba adaptor regs */
	uhp->uh_mr = (struct pte *)&nexus[0];
	/*
	 * The map registers start right at 20088000 on the
	 * ka630, so we have to subtract out the 2k offset to make the
	 * pointers work..
	 */
	uhp->uh_physuba = (struct uba_regs *)(((u_long)qb->qb_map)-0x800);

	uhp->uh_memsize = qb->qb_memsize;
	ioaccess(qb->qb_maddr, UMEMmap[numuba], uhp->uh_memsize * NBPG);
	uhp->uh_mem = umem[numuba];

	/*
	 * The I/O page is mapped to the 8K of the umem address space
	 * immediately after the memory section that is mapped.
	 */
	ioaccess(qb->qb_iopage, UMEMmap[numuba] + uhp->uh_memsize,
	    UBAIOPAGES * NBPG);
	uhp->uh_iopage = umem[numuba] + (uhp->uh_memsize * NBPG);

	unifind(uhp, qb->qb_iopage);
}
#endif

#if NUBA > 0
probeuba(vubp, pubp, pumem)
	struct uba_regs *vubp, *pubp;
	caddr_t pumem;
{
	register struct uba_hd *uhp = &uba_hd[numuba];

	/*
	 * Save virtual and physical addresses of adaptor.
	 */
	switch (cpu) {
#ifdef DW780
	case VAX_8600:
	case VAX_780:
		uhp->uh_type = DW780;
		break;
#endif
#ifdef DW750
	case VAX_750:
		uhp->uh_type = DW750;
		break;
#endif
#ifdef DW730
	case VAX_730:
		uhp->uh_type = DW730;
		break;
#endif
#ifdef DWBUA
	case VAX_8200:
		uhp->uh_type = DWBUA;
		break;
#endif
	default:
		panic("unknown UBA type");
		/*NOTREACHED*/
	}
	uhp->uh_uba = vubp;
	uhp->uh_physuba = pubp;
	uhp->uh_mr = vubp->uba_map;
	uhp->uh_memsize = UBAPAGES;

	ioaccess(pumem, UMEMmap[numuba], (UBAPAGES + UBAIOPAGES) * NBPG);
	uhp->uh_mem = umem[numuba];
	uhp->uh_iopage = umem[numuba] + (uhp->uh_memsize * NBPG);

	unifind(uhp, pumem + (uhp->uh_memsize * NBPG));
}

/*
 * Find devices on a UNIBUS.
 * Uses per-driver routine to set <br,cvec> into <r11,r10>,
 * and then fills in the tables, with help from a per-driver
 * slave initialization routine.
 */
unifind(uhp0, pumem)
	struct uba_hd *uhp0;
	caddr_t pumem;
{
#ifndef lint
	register int rbr, rcvec;			/* MUST BE r11, r10 */
#else
	/*
	 * Lint doesn't realize that these
	 * can be initialized asynchronously
	 * when devices interrupt.
	 */
	register int rbr = 0, rcvec = 0;
#endif
	register struct uba_device *ui;
	register struct uba_ctlr *um;
	register struct uba_hd *uhp = uhp0;
	u_short *reg, *ap, addr;
	struct uba_driver *udp;
	int i, (**ivec)();
	caddr_t ualloc;
	extern quad catcher[128];
	extern int br, cvec;
#if DW780 || DWBUA
	struct uba_regs *vubp = uhp->uh_uba;
#endif

	/*
	 * Initialize the UNIBUS, by freeing the map
	 * registers and the buffered data path registers
	 */
	uhp->uh_map = (struct map *)
		malloc((u_long)(UAMSIZ * sizeof (struct map)), M_DEVBUF,
		    M_NOWAIT);
	if (uhp->uh_map == 0)
		panic("no mem for unibus map");
	bzero((caddr_t)uhp->uh_map, (unsigned)(UAMSIZ * sizeof (struct map)));
	ubainitmaps(uhp);

	/*
	 * Initialize space for the UNIBUS interrupt vectors.
	 * On the 8600, can't use first slot in UNIvec
	 * (the vectors for the second SBI overlap it);
	 * move each set of vectors forward.
	 */
#if	VAX8600
	if (cpu == VAX_8600)
		uhp->uh_vec = UNIvec[numuba + 1];
	else
#endif
		uhp->uh_vec = UNIvec[numuba];
	for (i = 0; i < 128; i++)
		uhp->uh_vec[i] = scbentry(&catcher[i], SCB_ISTACK);
	/*
	 * Set last free interrupt vector for devices with
	 * programmable interrupt vectors.  Use is to decrement
	 * this number and use result as interrupt vector.
	 */
	uhp->uh_lastiv = 0x200;

#ifdef DWBUA
	if (uhp->uh_type == DWBUA)
		BUA(vubp)->bua_offset = (int)uhp->uh_vec - (int)&scb[0];
#endif

#ifdef DW780
	if (uhp->uh_type == DW780) {
		vubp->uba_sr = vubp->uba_sr;
		vubp->uba_cr = UBACR_IFS|UBACR_BRIE;
	}
#endif
	/*
	 * First configure devices that have unibus memory,
	 * allowing them to allocate the correct map registers.
	 */
	ubameminit(numuba);
	/*
	 * Grab some memory to record the umem address space we allocate,
	 * so we can be sure not to place two devices at the same address.
	 *
	 * We could use just 1/8 of this (we only want a 1 bit flag) but
	 * we are going to give it back anyway, and that would make the
	 * code here bigger (which we can't give back), so ...
	 *
	 * One day, someone will make a unibus with something other than
	 * an 8K i/o address space, & screw this totally.
	 */
	ualloc = (caddr_t)malloc((u_long)(8 * 1024), M_TEMP, M_NOWAIT);
	if (ualloc == (caddr_t)0)
		panic("no mem for unifind");
	bzero(ualloc, 8*1024);

	/*
	 * Map the first page of UNIBUS i/o
	 * space to the first page of memory
	 * for devices which will need to dma
	 * output to produce an interrupt.
	 */
	*(int *)(&uhp->uh_mr[0]) = UBAMR_MRV;

#define	ubaddr(uhp, off)    (u_short *)((int)(uhp)->uh_iopage + ubdevreg(off))
	/*
	 * Check each unibus mass storage controller.
	 * For each one which is potentially on this uba,
	 * see if it is really there, and if it is record it and
	 * then go looking for slaves.
	 */
	for (um = ubminit; udp = um->um_driver; um++) {
		if (um->um_ubanum != numuba && um->um_ubanum != '?' ||
		    um->um_alive)
			continue;
		addr = (u_short)um->um_addr;
		/*
		 * use the particular address specified first,
		 * or if it is given as "0", of there is no device
		 * at that address, try all the standard addresses
		 * in the driver til we find it
		 */
	    for (ap = udp->ud_addr; addr || (addr = *ap++); addr = 0) {

		if (ualloc[ubdevreg(addr)])
			continue;
		reg = ubaddr(uhp, addr);
		if (badaddr((caddr_t)reg, 2))
			continue;
#ifdef DW780
		if (uhp->uh_type == DW780 && vubp->uba_sr) {
			vubp->uba_sr = vubp->uba_sr;
			continue;
		}
#endif
		cvec = 0x200;
		rcvec = 0x200;
		i = (*udp->ud_probe)(reg, um->um_ctlr, um);
#ifdef DW780
		if (uhp->uh_type == DW780 && vubp->uba_sr) {
			vubp->uba_sr = vubp->uba_sr;
			continue;
		}
#endif
		if (i == 0)
			continue;
		printf("%s%d at uba%d csr %o ",
		    udp->ud_mname, um->um_ctlr, numuba, addr);
		if (rcvec == 0) {
			printf("zero vector\n");
			continue;
		}
		if (rcvec == 0x200) {
			printf("didn't interrupt\n");
			continue;
		}
		printf("vec %o, ipl %x\n", rcvec, rbr);
		csralloc(ualloc, addr, i);
		um->um_alive = 1;
		um->um_ubanum = numuba;
		um->um_hd = uhp;
		um->um_addr = (caddr_t)reg;
		udp->ud_minfo[um->um_ctlr] = um;
		for (rcvec /= 4, ivec = um->um_intr; *ivec; rcvec++, ivec++)
			uhp->uh_vec[rcvec] = scbentry(*ivec, SCB_ISTACK);
		for (ui = ubdinit; ui->ui_driver; ui++) {
			int t;

			if (ui->ui_driver != udp || ui->ui_alive ||
			    ui->ui_ctlr != um->um_ctlr && ui->ui_ctlr != '?' ||
			    ui->ui_ubanum != numuba && ui->ui_ubanum != '?')
				continue;
			t = ui->ui_ctlr;
			ui->ui_ctlr = um->um_ctlr;
			if ((*udp->ud_slave)(ui, reg) == 0)
				ui->ui_ctlr = t;
			else {
				ui->ui_alive = 1;
				ui->ui_ubanum = numuba;
				ui->ui_hd = uhp;
				ui->ui_addr = (caddr_t)reg;
				ui->ui_physaddr = pumem + ubdevreg(addr);
				if (ui->ui_dk && dkn < DK_NDRIVE)
					ui->ui_dk = dkn++;
				else
					ui->ui_dk = -1;
				ui->ui_mi = um;
				/* ui_type comes from driver */
				udp->ud_dinfo[ui->ui_unit] = ui;
				printf("%s%d at %s%d slave %d",
				    udp->ud_dname, ui->ui_unit,
				    udp->ud_mname, um->um_ctlr, ui->ui_slave);
				(*udp->ud_attach)(ui);
				printf("\n");
			}
		}
		break;
	    }
	}
	/*
	 * Now look for non-mass storage peripherals.
	 */
	for (ui = ubdinit; udp = ui->ui_driver; ui++) {
		if (ui->ui_ubanum != numuba && ui->ui_ubanum != '?' ||
		    ui->ui_alive || ui->ui_slave != -1)
			continue;
		addr = (u_short)ui->ui_addr;

	    for (ap = udp->ud_addr; addr || (addr = *ap++); addr = 0) {
		
		if (ualloc[ubdevreg(addr)])
			continue;
		reg = ubaddr(uhp, addr);
		if (badaddr((caddr_t)reg, 2))
			continue;
#ifdef DW780
		if (uhp->uh_type == DW780 && vubp->uba_sr) {
			vubp->uba_sr = vubp->uba_sr;
			continue;
		}
#endif
		rcvec = 0x200;
		cvec = 0x200;
		i = (*udp->ud_probe)(reg, ui);
#ifdef DW780
		if (uhp->uh_type == DW780 && vubp->uba_sr) {
			vubp->uba_sr = vubp->uba_sr;
			continue;
		}
#endif
		if (i == 0)
			continue;
		printf("%s%d at uba%d csr %o ",
		    ui->ui_driver->ud_dname, ui->ui_unit, numuba, addr);
		if (rcvec == 0) {
			printf("zero vector\n");
			continue;
		}
		if (rcvec == 0x200) {
			printf("didn't interrupt\n");
			continue;
		}
		printf("vec %o, ipl %x\n", rcvec, rbr);
		csralloc(ualloc, addr, i);
		ui->ui_hd = uhp;
		for (rcvec /= 4, ivec = ui->ui_intr; *ivec; rcvec++, ivec++)
			uhp->uh_vec[rcvec] = scbentry(*ivec, SCB_ISTACK);
		ui->ui_alive = 1;
		ui->ui_ubanum = numuba;
		ui->ui_addr = (caddr_t)reg;
		ui->ui_physaddr = pumem + ubdevreg(addr);
		ui->ui_dk = -1;
		/* ui_type comes from driver */
		udp->ud_dinfo[ui->ui_unit] = ui;
		(*udp->ud_attach)(ui);
		break;
	    }
	}

#ifdef DW780
	if (uhp->uh_type == DW780)
		uhp->uh_uba->uba_cr = UBACR_IFS | UBACR_BRIE |
		    UBACR_USEFIE | UBACR_SUEFIE |
		    (uhp->uh_uba->uba_cr & 0x7c000000);
#endif
	numuba++;

#ifdef	AUTO_DEBUG
	printf("Unibus allocation map");
	for (i = 0; i < 8*1024; ) {
		register n, m;

		if ((i % 128) == 0) {
			printf("\n%6o:", i);
			for (n = 0; n < 128; n++)
				if (ualloc[i+n])
					break;
			if (n == 128) {
				i += 128;
				continue;
			}
		}

		for (n = m = 0; n < 16; n++) {
			m <<= 1;
			m |= ualloc[i++];
		}

		printf(" %4x", m);
	}
	printf("\n");
#endif

	free(ualloc, M_TEMP);
}
#endif /* NUBA */

/*
 * Mark addresses starting at "addr" and continuing
 * "size" bytes as allocated in the map "ualloc".
 * Warn if the new allocation overlaps a previous allocation.
 */
static
csralloc(ualloc, addr, size)
	caddr_t ualloc;
	u_short addr;
	register int size;
{
	register caddr_t p;
	int warned = 0;

	p = &ualloc[ubdevreg(addr+size)];
	while (--size >= 0) {
		if (*--p && !warned) {
			printf(
	"WARNING: device registers overlap those for a previous device!\n");
			warned = 1;
		}
		*p = 1;
	}
}

/*
 * Make an IO register area accessible at physical address physa
 * by mapping kernel ptes starting at pte.
 */
ioaccess(physa, pte, size)
	caddr_t physa;
	register struct pte *pte;
	int size;
{
	register int i = btoc(size);
	register unsigned v = btop(physa);
	
	do
		*(int *)pte++ = PG_V|PG_KW|v++;
	while (--i > 0);
	mtpr(TBIA, 0);
}

/*
 * Configure swap space and related parameters.
 */
#ifndef SECSIZE
swapconf()
{
	register struct swdevt *swp;
	register int nblks;

	for (swp = swdevt; swp->sw_dev; swp++)
		if (bdevsw[major(swp->sw_dev)].d_psize) {
			nblks =
			  (*bdevsw[major(swp->sw_dev)].d_psize)(swp->sw_dev);
			if (nblks != -1 &&
			    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
		}
	dumpconf();
}
#else SECSIZE
swapconf()
{
	register struct swdevt *swp;
	register int nblks;
	register int bsize;
	struct partinfo dpart;

	for (swp = swdevt; swp->sw_dev; swp++)
		if ((nblks = psize(swp->sw_dev, &swp->sw_blksize,
		    &swp->sw_bshift)) != -1 &&
		    (swp->sw_nblks == 0 || swp->sw_nblks > nblks))
			swp->sw_nblks = nblks;

	if (!cold)	/* In case called for addition of another drive */
		return;
	if (dumplo == 0) {
		nblks = psize(dumpdev, (int *)0, (int *)0);
		if (nblks == -1 || nblks < ctod(physmem))
			dumplo = 0;
		else
			dumplo = nblks - ctod(physmem);
	}
}

/*
 * Return size of disk partition in DEV_BSIZE units.
 * If needed, return sector size.
 */
psize(dev, psize, pshift)
	register dev_t dev;
	int *psize, *pshift;
{
	register int nblks, bsize, bshift;
	struct partinfo dpart;

	if ((*bdevsw[major(dev)].d_ioctl)(dev, DIOCGPART,
	    (caddr_t)&dpart, FREAD) == 0)
		bsize = dpart.disklab->d_secsize;
	else
		bsize = DEV_BSIZE;
	if (psize)
		*psize = bsize;
	bshift = 0;
	for (nblks = DEV_BSIZE / bsize; nblks > 1; nblks >>= 1)
		bshift++;
	if (pshift)
		*pshift = bshift;
	nblks = -1;
	if (bdevsw[major(dev)].d_psize) {
		nblks = (*bdevsw[major(dev)].d_psize)(dev);
		if (nblks != -1)
			nblks >>= bshift;
	}
	return (nblks);
}
#endif SECSIZE

#define	DOSWAP			/* Change swdevt, argdev, and dumpdev too */
u_long	bootdev;		/* should be dev_t, but not until 32 bits */

static	char devname[][2] = {
	'h','p',	/* 0 = hp */
	0,0,		/* 1 = ht */
	'u','p',	/* 2 = up */
	'r','k',	/* 3 = hk */
	0,0,		/* 4 = sw */
	0,0,		/* 5 = tm */
	0,0,		/* 6 = ts */
	0,0,		/* 7 = mt */
	0,0,		/* 8 = tu */
	'r','a',	/* 9 = ra */
	0,0,		/* 10 = ut */
	'r','b',	/* 11 = rb */
	0,0,		/* 12 = uu */
	0,0,		/* 13 = rx */
	'r','l',	/* 14 = rl */
	0,0,		/* 15 = tmscp */
	'k','r',	/* 16 = ra on kdb50 */
};

#define	PARTITIONMASK	0x7
#define	PARTITIONSHIFT	3

/*
 * Attempt to find the device from which we were booted.
 * If we can do so, and not instructed not to do so,
 * change rootdev to correspond to the load device.
 */
setroot()
{
	int  majdev, mindev, unit, part, controller, adaptor;
	dev_t temp, orootdev;
	struct swdevt *swp;

	if (boothowto & RB_DFLTROOT ||
	    (bootdev & B_MAGICMASK) != (u_long)B_DEVMAGIC)
		return;
	majdev = B_TYPE(bootdev);
	if (majdev >= sizeof(devname) / sizeof(devname[0]))
		return;
	adaptor = B_ADAPTOR(bootdev);
	controller = B_CONTROLLER(bootdev);
	part = B_PARTITION(bootdev);
	unit = B_UNIT(bootdev);
	if (majdev == 0) {	/* MBA device */
#if NMBA > 0
		register struct mba_device *mbap;
		int mask;

/*
 * The MBA number used at boot time is not necessarily the same as the
 * MBA number used by the kernel.  In order to change the rootdev we need to
 * convert the boot MBA number to the kernel MBA number.  The address space
 * for an MBA used by the boot code is 0x20010000 + 0x2000 * MBA_number
 * on the 78? and 86?0, 0xf28000 + 0x2000 * MBA_number on the 750.
 * Therefore we can search the mba_hd table for the MBA that has the physical
 * address corresponding to the boot MBA number.
 */
#define	PHYSADRSHFT	13
#define	PHYSMBAMASK780	0x7
#define	PHYSMBAMASK750	0x3

		switch (cpu) {

		case VAX_780:
		case VAX_8600:
		default:
			mask = PHYSMBAMASK780;
			break;

		case VAX_750:
			mask = PHYSMBAMASK750;
			break;
		}
		for (mbap = mbdinit; mbap->mi_driver; mbap++)
			if (mbap->mi_alive && mbap->mi_drive == unit &&
			    (((long)mbap->mi_hd->mh_physmba >> PHYSADRSHFT)
			      & mask) == adaptor)
			    	break;
		if (mbap->mi_driver == 0)
			return;
		mindev = mbap->mi_unit;
#else
		return;
#endif
	} else {
		register struct uba_device *ubap;

		for (ubap = ubdinit; ubap->ui_driver; ubap++)
			if (ubap->ui_alive && ubap->ui_slave == unit &&
			   ubap->ui_ctlr == controller &&
			   ubap->ui_ubanum == adaptor &&
			   ubap->ui_driver->ud_dname[0] == devname[majdev][0] &&
			   ubap->ui_driver->ud_dname[1] == devname[majdev][1])
			    	break;

		if (ubap->ui_driver == 0)
			return;
		mindev = ubap->ui_unit;
	}
	mindev = (mindev << PARTITIONSHIFT) + part;
	orootdev = rootdev;
	rootdev = makedev(majdev, mindev);
	/*
	 * If the original rootdev is the same as the one
	 * just calculated, don't need to adjust the swap configuration.
	 */
	if (rootdev == orootdev)
		return;

	printf("Changing root device to %c%c%d%c\n",
		devname[majdev][0], devname[majdev][1],
		mindev >> PARTITIONSHIFT, part + 'a');

#ifdef DOSWAP
	mindev &= ~PARTITIONMASK;
	for (swp = swdevt; swp->sw_dev; swp++) {
		if (majdev == major(swp->sw_dev) &&
		    mindev == (minor(swp->sw_dev) & ~PARTITIONMASK)) {
			temp = swdevt[0].sw_dev;
			swdevt[0].sw_dev = swp->sw_dev;
			swp->sw_dev = temp;
			break;
		}
	}
	if (swp->sw_dev == 0)
		return;

	/*
	 * If argdev and dumpdev were the same as the old primary swap
	 * device, move them to the new primary swap device.
	 */
	if (temp == dumpdev)
		dumpdev = swdevt[0].sw_dev;
	if (temp == argdev)
		argdev = swdevt[0].sw_dev;
#endif
}
