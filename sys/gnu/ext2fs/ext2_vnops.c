/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ext2_vnops.c	8.7 (Berkeley) 2/3/94
 */

#if !defined(__FreeBSD__)
#include "fifo.h"
#include "diagnostic.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#if !defined(__FreeBSD__)
#include <ufs/ufs/lockf.h>
#else
#include <sys/signalvar.h>
#endif
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/ufs_extern.h>
#include <ufs/ffs/ffs_extern.h>

#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_extern.h>

static int ext2_fsync __P((struct vop_fsync_args *));
static int ext2_read __P((struct vop_read_args *));
static int ext2_write __P((struct vop_write_args *));

/* Global vfs data structures for ufs. */
vop_t **ext2_vnodeop_p;
static struct vnodeopv_entry_desc ext2_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperate },
	{ &vop_cachedlookup_desc,	(vop_t *) ext2_lookup },
	{ &vop_fsync_desc,		(vop_t *) ext2_fsync },
	{ &vop_inactive_desc,		(vop_t *) ext2_inactive },
	{ &vop_lookup_desc,		(vop_t *) vfs_cache_lookup },
	{ &vop_read_desc,		(vop_t *) ext2_read },
	{ &vop_readdir_desc,		(vop_t *) ext2_readdir },
	{ &vop_reallocblks_desc,	(vop_t *) ext2_reallocblks },
	{ &vop_write_desc,		(vop_t *) ext2_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc ext2fs_vnodeop_opv_desc =
	{ &ext2_vnodeop_p, ext2_vnodeop_entries };

vop_t **ext2_specop_p;
static struct vnodeopv_entry_desc ext2_specop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatespec },
	{ &vop_fsync_desc,		(vop_t *) ext2_fsync },
	{ &vop_inactive_desc,		(vop_t *) ext2_inactive },
	{ NULL, NULL }
};
static struct vnodeopv_desc ext2fs_specop_opv_desc =
	{ &ext2_specop_p, ext2_specop_entries };

vop_t **ext2_fifoop_p;
static struct vnodeopv_entry_desc ext2_fifoop_entries[] = {
	{ &vop_default_desc,		(vop_t *) ufs_vnoperatefifo },
	{ &vop_fsync_desc,		(vop_t *) ext2_fsync },
	{ &vop_inactive_desc,		(vop_t *) ext2_inactive },
	{ NULL, NULL }
};
static struct vnodeopv_desc ext2fs_fifoop_opv_desc =
	{ &ext2_fifoop_p, ext2_fifoop_entries };

#if defined(__FreeBSD__)
	VNODEOP_SET(ext2fs_vnodeop_opv_desc);
	VNODEOP_SET(ext2fs_specop_opv_desc);
	VNODEOP_SET(ext2fs_fifoop_opv_desc);
#endif

#include <gnu/ext2fs/ext2_readwrite.c>

/*
 * Synch an open file.
 */
/* ARGSUSED */
static int
ext2_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct buf *bp;
	struct timeval tv;
	struct buf *nbp;
	int s;

	/* 
	 * Clean memory object.
	 * XXX add this to all file systems.
	 * XXX why is all this fs specific?
	 */
#if !defined(__FreeBSD__)
	vn_pager_sync(vp, ap->a_waitfor);
#endif

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
	ext2_discard_prealloc(VTOI(vp));

loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if ((bp->b_flags & B_BUSY))
			continue;
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("ext2_fsync: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		/*
		 * Wait for I/O associated with indirect blocks to complete,
		 * since there is no way to quickly wait for them below.
		 */
		if (bp->b_vp == vp || ap->a_waitfor == MNT_NOWAIT)
			(void) bawrite(bp);
		else
			(void) bwrite(bp);
		goto loop;
	}
	if (ap->a_waitfor == MNT_WAIT) {
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
#if !defined(__FreeBSD__)
			sleep((caddr_t)&vp->v_numoutput, PRIBIO + 1);
#else
			tsleep((caddr_t)&vp->v_numoutput, PRIBIO + 1, "extfsn", 0);
#endif
		}
#if DIAGNOSTIC
		if (vp->v_dirtyblkhd.lh_first) {
			vprint("ext2_fsync: dirty", vp);
			goto loop;
		}
#endif
	}
	splx(s);
	gettime(&tv);
	return (UFS_UPDATE(ap->a_vp, &tv, &tv, ap->a_waitfor == MNT_WAIT));
}
