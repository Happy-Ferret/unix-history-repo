/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)kern_sysctl.c	7.4 (Berkeley) %G%
 */

#include "param.h"
#include "user.h"
#include "proc.h"
#include "kinfo.h"
#include "vm.h"
#include "ioctl.h"
#include "tty.h"
#include "buf.h"


#define snderr(e) { error = (e); goto release;}
extern int kinfo_doproc(), kinfo_rtable();
struct kinfo_lock kinfo_lock;

getkinfo()
{
	register struct a {
		int	op;
		char	*where;
		int	*size;
		int	arg;
	} *uap = (struct a *)u.u_ap;
	int wanted, (*server)(), error = 0;
	int	bufsize,	/* max size of users buffer */
		copysize, 	/* size copied */
		needed,	
		locked;

	while (kinfo_lock.kl_lock) {
		kinfo_lock.kl_want++;
		sleep(&kinfo_lock, PRIBIO+1);
		kinfo_lock.kl_want--;
		kinfo_lock.kl_locked++;
	}
	kinfo_lock.kl_lock++;

	switch (ki_type(uap->op)) {

	case KINFO_PROC:
		server = kinfo_doproc;
		break;

	case KINFO_RT:
		server = kinfo_rtable;
		break;

	default:
		snderr(EINVAL);
	}
	if (error = (*server)(uap->op, NULL, NULL, uap->arg, &needed))
		goto release;
	if (uap->where == NULL || uap->size == NULL)
		goto release;  /* only want estimate of bufsize */
	if (error = copyin((caddr_t)uap->size,
				(caddr_t)&bufsize, sizeof (bufsize)))
		goto release;
	locked = copysize = MIN(needed, bufsize);
	if (!useracc(uap->where, copysize, B_WRITE))
		snderr(EFAULT);
	/*
	 * lock down target pages - NEED DEADLOCK AVOIDANCE
	 */
	if (copysize > ((int)ptob(freemem) - (20 * 1024))) 	/* XXX */
		snderr(ENOMEM);
	vslock(uap->where, copysize);
	error = (*server)(uap->op, uap->where, &copysize, uap->arg, &needed);
	vsunlock(uap->where, locked, B_WRITE);
	if (error)
		goto release;
	error = copyout((caddr_t)&copysize,
				(caddr_t)uap->size, sizeof (copysize));

release:
	kinfo_lock.kl_lock--;
	if (kinfo_lock.kl_want)
		wakeup(&kinfo_lock);
	if (error)
		u.u_error = error;
	else
		u.u_r.r_val1 = needed;
}

/* 
 * try over estimating by 5 procs 
 */
#define KINFO_PROCSLOP	(5 * sizeof (struct kinfo_proc))

int kinfo_proc_userfailed;
int kinfo_proc_wefailed;

kinfo_doprocs(op, where, acopysize, arg, aneeded)
	char *where;
	int *acopysize, *aneeded;
{
	register struct proc *p;
	register caddr_t dp = (caddr_t)where;
	register needed = 0;
	int buflen;
	int doingzomb;
	struct eproc eproc;
	struct tty *tp;
	int error = 0;

	if (where != NULL)
		buflen = *acopysize;

	p = allproc;
	doingzomb = 0;
again:
	for (; p != NULL; p = p->p_nxt) {
		/* 
		 * TODO - make more efficient (see notes below).
		 * do by session. 
		 */
		switch (ki_op(op)) {

		case KINFO_PROC_PID:
			/* could do this with just a lookup */
			if (p->p_pid != (pid_t)arg)
				continue;
			break;

		case KINFO_PROC_PGRP:
			/* could do this by traversing pgrp */
			if (p->p_pgrp->pg_id != (pid_t)arg)
				continue;
			break;

		case KINFO_PROC_TTY:
			if ((p->p_flag&SCTTY) == 0 || 
			    p->p_session->s_ttyp == NULL ||
			    p->p_session->s_ttyp->t_dev != (dev_t)arg)
				continue;
			break;

		case KINFO_PROC_UID:
			if (p->p_uid != (uid_t)arg)
				continue;
			break;

		case KINFO_PROC_RUID:
			if (p->p_ruid != (uid_t)arg)
				continue;
			break;
		}
		if (where != NULL && buflen >= sizeof (struct kinfo_proc)) {
			if (error = copyout((caddr_t)p, dp, 
			    sizeof (struct proc)))
				return (error);
			dp += sizeof (struct proc);
			eproc.kp_paddr = p;
			eproc.kp_sess = p->p_pgrp->pg_session;
			eproc.kp_pgid = p->p_pgrp->pg_id;
			eproc.kp_jobc = p->p_pgrp->pg_jobc;
			tp = p->p_pgrp->pg_session->s_ttyp;
			if ((p->p_flag&SCTTY) && tp != NULL) {
				eproc.kp_tdev = tp->t_dev;
				eproc.kp_tpgid = tp->t_pgrp ? 
					tp->t_pgrp->pg_id : -1;
				eproc.kp_tsess = tp->t_session;
			} else
				eproc.kp_tdev = NODEV;
			if (error = copyout((caddr_t)&eproc, dp, 
			    sizeof (eproc)))
				return (error);
			dp += sizeof (eproc);
			buflen -= sizeof (struct kinfo_proc);
		}
		needed += sizeof (struct kinfo_proc);
	}
	if (doingzomb == 0) {
		p = zombproc;
		doingzomb++;
		goto again;
	}
	if (where != NULL)
		*acopysize = dp - where;
	else
		needed += KINFO_PROCSLOP;
	*aneeded = needed;

	return (0);
}
