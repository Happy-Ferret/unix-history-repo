/*-
 * Copyright (c) 1992 OMRON Corporation.
 * Copyright (c) 1991, 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 * from: hp300/hp300/conf.c	7.13 (Berkeley) 7/9/92
 *
 *	@(#)conf.c	7.2 (Berkeley) %G%
 */

#include "sys/param.h"
#include "sys/systm.h"
#include "sys/buf.h"
#include "sys/ioctl.h"
#include "sys/proc.h"
#include "sys/tty.h"
#include "sys/conf.h"

int	rawread		__P((dev_t, struct uio *, int));
int	rawwrite	__P((dev_t, struct uio *, int));
int	swstrategy	__P((struct buf *));
int	ttselect	__P((dev_t, int, struct proc *));

#define	dev_type_open(n)	int n __P((dev_t, int, int, struct proc *))
#define	dev_type_close(n)	int n __P((dev_t, int, int, struct proc *))
#define	dev_type_strategy(n)	int n __P((struct buf *))
#define	dev_type_ioctl(n) \
	int n __P((dev_t, int, caddr_t, int, struct proc *))

/* bdevsw-specific types */
#define	dev_type_dump(n)	int n __P(())
#define	dev_type_size(n)	int n __P((dev_t))

#define	dev_decl(n,t)	__CONCAT(dev_type_,t)(__CONCAT(n,t))
#define	dev_init(c,n,t) \
	(c > 0 ? __CONCAT(n,t) : (__CONCAT(dev_type_,t)((*))) enxio)

/* bdevsw-specific initializations */
#define	dev_size_init(c,n)	(c > 0 ? __CONCAT(n,size) : 0)

#define	bdev_decl(n) \
	dev_decl(n,open); dev_decl(n,close); dev_decl(n,strategy); \
	dev_decl(n,ioctl); dev_decl(n,dump); dev_decl(n,size)

#define	bdev_disk_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, \
	dev_init(c,n,strategy), dev_init(c,n,ioctl), \
	dev_init(c,n,dump), dev_size_init(c,n), 0 }

#define	bdev_tape_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), \
	dev_init(c,n,strategy), dev_init(c,n,ioctl), \
	dev_init(c,n,dump), 0, B_TAPE }

#define	bdev_swap_init() { \
	(dev_type_open((*))) enodev, (dev_type_close((*))) enodev, \
	swstrategy, (dev_type_ioctl((*))) enodev, \
	(dev_type_dump((*))) enodev, 0, 0 }

#define	bdev_notdef()	bdev_tape_init(0,no)
bdev_decl(no);	/* dummy declarations */

#include "st.h"
#include "sd.h"
#include "vn.h"

bdev_decl(st);
bdev_decl(sd);
bdev_decl(vn);

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),		/* 0 */
	bdev_notdef(),		/* 1 */
	bdev_notdef(),		/* 2: ram disk */
	bdev_swap_init(),	/* 3: swap pseudo-device */
	bdev_disk_init(NSD,sd),	/* 4: scsi disk */
	bdev_notdef(),		/* 5 */
	bdev_disk_init(NVN,vn),	/* 6: vnode disk driver (swap to files) */
	bdev_tape_init(NST,st),	/* 7: scsi tape */
};

int	nblkdev = sizeof (bdevsw) / sizeof (bdevsw[0]);

/* cdevsw-specific types */
#define	dev_type_read(n)	int n __P((dev_t, struct uio *, int))
#define	dev_type_write(n)	int n __P((dev_t, struct uio *, int))
#define	dev_type_stop(n)	int n __P((struct tty *, int))
#define	dev_type_reset(n)	int n __P((int))
#define	dev_type_select(n)	int n __P((dev_t, int, struct proc *))
#define	dev_type_map(n)	int n __P(())

#define	cdev_decl(n) \
	dev_decl(n,open); dev_decl(n,close); dev_decl(n,read); \
	dev_decl(n,write); dev_decl(n,ioctl); dev_decl(n,stop); \
	dev_decl(n,reset); dev_decl(n,select); dev_decl(n,map); \
	dev_decl(n,strategy); extern struct tty __CONCAT(n,_tty)[]

#define	dev_tty_init(c,n)	(c > 0 ? __CONCAT(n,_tty) : 0)

/* open, read, write, ioctl, strategy */
#define	cdev_disk_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, \
	dev_init(c,n,strategy) }

/* open, close, read, write, ioctl, strategy */
#define	cdev_tape_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, \
	dev_init(c,n,strategy) }

/* open, close, read, write, ioctl, stop, tty */
#define	cdev_tty_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	(dev_type_reset((*))) nullop, dev_tty_init(c,n), ttselect, \
	(dev_type_map((*))) enodev, 0 }

#define	cdev_notdef() { \
	(dev_type_open((*))) enodev, (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, \
	(dev_type_map((*))) enodev, 0 }

cdev_decl(no);			/* dummy declarations */

cdev_decl(cn);
/* open, close, read, write, ioctl, select -- XXX should be a tty */
#define	cdev_cn_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, 0, dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

cdev_decl(ctty);
/* open, read, write, ioctl, select -- XXX should be a tty */
#define	cdev_ctty_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, 0, dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

dev_type_read(mmrw);
/* read/write */
#define	cdev_mm_init(c,n) { \
	(dev_type_open((*))) nullop, (dev_type_close((*))) nullop, mmrw, \
	mmrw, (dev_type_ioctl((*))) enodev, (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, 0 }

/* read, write, strategy */
#define	cdev_swap_init(c,n) { \
	(dev_type_open((*))) nullop, (dev_type_close((*))) nullop, rawread, \
	rawwrite, (dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, (dev_type_select((*))) enodev, \
	(dev_type_map((*))) enodev, dev_init(c,n,strategy) }

#include "pty.h"
#define	pts_tty		pt_tty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptc_tty		pt_tty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);

/* open, close, read, write, ioctl, tty, select */
#define	cdev_ptc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, dev_tty_init(c,n), dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

cdev_decl(log);
/* open, close, read, ioctl, select -- XXX should be a generic device */
#define	cdev_log_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, (dev_type_reset((*))) nullop, 0, \
	dev_init(c,n,select), (dev_type_map((*))) enodev, 0 }

cdev_decl(st);
cdev_decl(sd);

#include "sio.h"
cdev_decl(sio);

cdev_decl(cd);

cdev_decl(vn);
/* open, read, write, ioctl -- XXX should be a disk */
#define	cdev_vn_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, \
	0 }

dev_type_open(fdopen);
/* open */
#define	cdev_fd_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_map((*))) enodev, 0 }

#include "bpfilter.h"
cdev_decl(bpf);
/* open, close, read, write, ioctl, select -- XXX should be generic device */
#define	cdev_bpf_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) enodev, 0, dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_ctty_init(1,ctty),		/* 1: controlling terminal */
	cdev_mm_init(1,mm),		/* 2: /dev/{null,mem,kmem,...} */
	cdev_swap_init(1,sw),		/* 3: /dev/drum (swap pseudo-device) */
	cdev_tty_init(NPTY,pts),	/* 4: pseudo-tty slave */
	cdev_ptc_init(NPTY,ptc),	/* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_notdef(),			/* 7 */
	cdev_disk_init(NSD,sd),		/* 8: scsi disk */
	cdev_notdef(),			/* 9: ram disk */
	cdev_notdef(),			/* 10 */
	cdev_notdef(),			/* 11 */
	cdev_tty_init(NSIO,sio),	/* 12: built-in single-port serial */
	cdev_notdef(),			/* 13 */
	cdev_notdef(),			/* 14 */
	cdev_notdef(),			/* 15 */
	cdev_notdef(),			/* 16 */
	cdev_notdef(),			/* 17 */
	cdev_notdef(),			/* 18 */
	cdev_vn_init(NVN,vn),		/* 19: vnode disk */
	cdev_tape_init(NST,st),		/* 20: scsi tape */
	cdev_fd_init(1,fd),		/* 21: file descriptor pseudo-dev */
	cdev_bpf_init(NBPFILTER,bpf),	/* 22: berkeley packet filter */
};

int	nchrdev = sizeof (cdevsw) / sizeof (cdevsw[0]);

int	mem_no = 2; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(3, 0);
