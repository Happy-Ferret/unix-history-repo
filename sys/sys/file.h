/*-
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
 *	@(#)file.h	8.3 (Berkeley) 1/9/95
 * $FreeBSD$
 */

#ifndef _SYS_FILE_H_
#define	_SYS_FILE_H_

#ifndef _KERNEL
#include <sys/types.h> /* XXX */
#include <sys/fcntl.h>
#include <sys/unistd.h>
#else
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>

struct stat;
struct thread;
struct uio;
struct knote;
struct vnode;
struct socket;


#endif /* _KERNEL */

#define	DTYPE_VNODE	1	/* file */
#define	DTYPE_SOCKET	2	/* communications endpoint */
#define	DTYPE_PIPE	3	/* pipe */
#define	DTYPE_FIFO	4	/* fifo (named pipe) */
#define	DTYPE_KQUEUE	5	/* event queue */
#define	DTYPE_CRYPTO	6	/* crypto */
#define	DTYPE_MQUEUE	7	/* posix message queue */
#define	DTYPE_SHM	8	/* swap-backed shared memory */
#define	DTYPE_SEM	9	/* posix semaphore */
#define	DTYPE_PTS	10	/* pseudo teletype master device */
#define	DTYPE_DEV	11	/* Device specific fd type */
#define	DTYPE_CAPABILITY	12	/* capability */
#define	DTYPE_PROCDESC	13	/* process descriptor */

#ifdef _KERNEL

struct file;
struct ucred;

typedef int fo_rdwr_t(struct file *fp, struct uio *uio,
		    struct ucred *active_cred, int flags,
		    struct thread *td);
#define	FOF_OFFSET	1	/* Use the offset in uio argument */
typedef	int fo_truncate_t(struct file *fp, off_t length,
		    struct ucred *active_cred, struct thread *td);
typedef	int fo_ioctl_t(struct file *fp, u_long com, void *data,
		    struct ucred *active_cred, struct thread *td);
typedef	int fo_poll_t(struct file *fp, int events,
		    struct ucred *active_cred, struct thread *td);
typedef	int fo_kqfilter_t(struct file *fp, struct knote *kn);
typedef	int fo_stat_t(struct file *fp, struct stat *sb,
		    struct ucred *active_cred, struct thread *td);
typedef	int fo_close_t(struct file *fp, struct thread *td);
typedef	int fo_chmod_t(struct file *fp, mode_t mode,
		    struct ucred *active_cred, struct thread *td);
typedef	int fo_chown_t(struct file *fp, uid_t uid, gid_t gid,
		    struct ucred *active_cred, struct thread *td);
typedef	int fo_flags_t;

struct fileops {
	fo_rdwr_t	*fo_read;
	fo_rdwr_t	*fo_write;
	fo_truncate_t	*fo_truncate;
	fo_ioctl_t	*fo_ioctl;
	fo_poll_t	*fo_poll;
	fo_kqfilter_t	*fo_kqfilter;
	fo_stat_t	*fo_stat;
	fo_close_t	*fo_close;
	fo_chmod_t	*fo_chmod;
	fo_chown_t	*fo_chown;
	fo_flags_t	fo_flags;	/* DFLAG_* below */
};

#define DFLAG_PASSABLE	0x01	/* may be passed via unix sockets. */
#define DFLAG_SEEKABLE	0x02	/* seekable / nonsequential */
#endif /* _KERNEL */

#if defined(_KERNEL) || defined(_WANT_FILE)
/*
 * Kernel descriptor table.
 * One entry for each open kernel vnode and socket.
 *
 * Below is the list of locks that protects members in struct file.
 *
 * (f) protected with mtx_lock(mtx_pool_find(fp))
 * (d) cdevpriv_mtx
 * none	not locked
 */

struct fadvise_info {
	int		fa_advice;	/* (f) FADV_* type. */
	off_t		fa_start;	/* (f) Region start. */
	off_t		fa_end;		/* (f) Region end. */
	off_t		fa_prevstart;	/* (f) Previous NOREUSE start. */
	off_t		fa_prevend;	/* (f) Previous NOREUSE end. */
};

struct file {
	void		*f_data;	/* file descriptor specific data */
	struct fileops	*f_ops;		/* File operations */
	struct ucred	*f_cred;	/* associated credentials. */
	struct vnode 	*f_vnode;	/* NULL or applicable vnode */
	short		f_type;		/* descriptor type */
	short		f_vnread_flags; /* (f) Sleep lock for f_offset */
	volatile u_int	f_flag;		/* see fcntl.h */
	volatile u_int 	f_count;	/* reference count */
	/*
	 *  DTYPE_VNODE specific fields.
	 */
	int		f_seqcount;	/* Count of sequential accesses. */
	off_t		f_nextoff;	/* next expected read/write offset. */
	union {
		struct cdev_privdata *fvn_cdevpriv;
					/* (d) Private data for the cdev. */
		struct fadvise_info *fvn_advice;
	} f_vnun;
	/*
	 *  DFLAG_SEEKABLE specific fields
	 */
	off_t		f_offset;
	/*
	 * Mandatory Access control information.
	 */
	void		*f_label;	/* Place-holder for MAC label. */
};

#define	f_cdevpriv	f_vnun.fvn_cdevpriv
#define	f_advice	f_vnun.fvn_advice

#define	FOFFSET_LOCKED       0x1
#define	FOFFSET_LOCK_WAITING 0x2		 

#endif /* _KERNEL || _WANT_FILE */

/*
 * Userland version of struct file, for sysctl
 */
struct xfile {
	size_t	xf_size;	/* size of struct xfile */
	pid_t	xf_pid;		/* owning process */
	uid_t	xf_uid;		/* effective uid of owning process */
	int	xf_fd;		/* descriptor number */
	void	*xf_file;	/* address of struct file */
	short	xf_type;	/* descriptor type */
	int	xf_count;	/* reference count */
	int	xf_msgcount;	/* references from message queue */
	off_t	xf_offset;	/* file offset */
	void	*xf_data;	/* file descriptor specific data */
	void	*xf_vnode;	/* vnode pointer */
	u_int	xf_flag;	/* flags (see fcntl.h) */
};

#ifdef _KERNEL

extern struct fileops vnops;
extern struct fileops badfileops;
extern struct fileops socketops;
extern int maxfiles;		/* kernel limit on number of open files */
extern int maxfilesperproc;	/* per process limit on number of open files */
extern volatile int openfiles;	/* actual number of open files */

int fget(struct thread *td, int fd, cap_rights_t rights, struct file **fpp);
int fget_mmap(struct thread *td, int fd, cap_rights_t rights,
    u_char *maxprotp, struct file **fpp);
int fget_read(struct thread *td, int fd, cap_rights_t rights,
    struct file **fpp);
int fget_write(struct thread *td, int fd, cap_rights_t rights,
    struct file **fpp);
int fgetcap(struct thread *td, int fd, struct file **fpp);
int _fdrop(struct file *fp, struct thread *td);

/*
 * The socket operations are used a couple of places.
 * XXX: This is wrong, they should go through the operations vector for
 * XXX: sockets instead of going directly for the individual functions. /phk
 */
fo_rdwr_t	soo_read;
fo_rdwr_t	soo_write;
fo_truncate_t	soo_truncate;
fo_ioctl_t	soo_ioctl;
fo_poll_t	soo_poll;
fo_kqfilter_t	soo_kqfilter;
fo_stat_t	soo_stat;
fo_close_t	soo_close;

fo_chmod_t	invfo_chmod;
fo_chown_t	invfo_chown;

void finit(struct file *, u_int, short, void *, struct fileops *);
int fgetvp(struct thread *td, int fd, cap_rights_t rights, struct vnode **vpp);
int fgetvp_rights(struct thread *td, int fd, cap_rights_t need,
    cap_rights_t *have, struct vnode **vpp);
int fgetvp_read(struct thread *td, int fd, cap_rights_t rights,
    struct vnode **vpp);
int fgetvp_write(struct thread *td, int fd, cap_rights_t rights,
    struct vnode **vpp);

int fgetsock(struct thread *td, int fd, cap_rights_t rights,
    struct socket **spp, u_int *fflagp);
void fputsock(struct socket *sp);

static __inline int
_fnoop(void)
{

	return (0);
}

#define	fhold(fp)							\
	(refcount_acquire(&(fp)->f_count))
#define	fdrop(fp, td)							\
	(refcount_release(&(fp)->f_count) ? _fdrop((fp), (td)) : _fnoop())

static __inline fo_rdwr_t	fo_read;
static __inline fo_rdwr_t	fo_write;
static __inline fo_truncate_t	fo_truncate;
static __inline fo_ioctl_t	fo_ioctl;
static __inline fo_poll_t	fo_poll;
static __inline fo_kqfilter_t	fo_kqfilter;
static __inline fo_stat_t	fo_stat;
static __inline fo_close_t	fo_close;
static __inline fo_chmod_t	fo_chmod;
static __inline fo_chown_t	fo_chown;

static __inline int
fo_read(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return ((*fp->f_ops->fo_read)(fp, uio, active_cred, flags, td));
}

static __inline int
fo_write(struct file *fp, struct uio *uio, struct ucred *active_cred,
    int flags, struct thread *td)
{

	return ((*fp->f_ops->fo_write)(fp, uio, active_cred, flags, td));
}

static __inline int
fo_truncate(struct file *fp, off_t length, struct ucred *active_cred,
    struct thread *td)
{

	return ((*fp->f_ops->fo_truncate)(fp, length, active_cred, td));
}

static __inline int
fo_ioctl(struct file *fp, u_long com, void *data, struct ucred *active_cred,
    struct thread *td)
{

	return ((*fp->f_ops->fo_ioctl)(fp, com, data, active_cred, td));
}

static __inline int
fo_poll(struct file *fp, int events, struct ucred *active_cred,
    struct thread *td)
{

	return ((*fp->f_ops->fo_poll)(fp, events, active_cred, td));
}

static __inline int
fo_stat(struct file *fp, struct stat *sb, struct ucred *active_cred,
    struct thread *td)
{

	return ((*fp->f_ops->fo_stat)(fp, sb, active_cred, td));
}

static __inline int
fo_close(struct file *fp, struct thread *td)
{

	return ((*fp->f_ops->fo_close)(fp, td));
}

static __inline int
fo_kqfilter(struct file *fp, struct knote *kn)
{

	return ((*fp->f_ops->fo_kqfilter)(fp, kn));
}

static __inline int
fo_chmod(struct file *fp, mode_t mode, struct ucred *active_cred,
    struct thread *td)
{

	return ((*fp->f_ops->fo_chmod)(fp, mode, active_cred, td));
}

static __inline int
fo_chown(struct file *fp, uid_t uid, gid_t gid, struct ucred *active_cred,
    struct thread *td)
{

	return ((*fp->f_ops->fo_chown)(fp, uid, gid, active_cred, td));
}

#endif /* _KERNEL */

#endif /* !SYS_FILE_H */
