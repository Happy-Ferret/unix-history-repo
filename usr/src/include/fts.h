/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)fts.h	5.17 (Berkeley) %G%
 */

#ifndef	_FTS_H_
#define	_FTS_H_

typedef struct {
	struct _ftsent *fts_cur;	/* current node */
	struct _ftsent *fts_child;	/* linked list of children */
	struct _ftsent **fts_array;	/* sort array */
	dev_t rdev;			/* starting device # */
	char *fts_path;			/* path for this descent */
	int fts_rfd;			/* fd for root */
	int fts_pathlen;		/* sizeof(path) */
	int fts_nitems;			/* elements in the sort array */
	int (*fts_compar)();		/* compare function */

#define	FTS_LOGICAL	0x001		/* logical walk */
#define	FTS_NOCHDIR	0x002		/* don't change directories */
#define	FTS_NOSTAT	0x004		/* don't get stat info */
#define	FTS_PHYSICAL	0x008		/* physical walk */
#define	FTS_SEEDOT	0x010		/* return dot and dot-dot */
#define	FTS_STOP	0x020		/* (private) unrecoverable error */
#define	FTS_XDEV	0x040		/* don't cross devices */
	int fts_options;		/* openfts() options */
} FTS;

typedef struct _ftsent {
	struct _ftsent *fts_cycle;	/* cycle node */
	struct _ftsent *fts_parent;	/* parent directory */
	struct _ftsent *fts_link;	/* next file in directory */
	union {
		long number;		/* local numeric value */
		void *pointer;		/* local address value */
	} fts_local;
#define	fts_number	fts_local.number
#define	fts_pointer	fts_local.pointer
	char *fts_accpath;		/* access path */
	char *fts_path;			/* root path */
	int fts_cderr;			/* chdir failed -- errno */
	short fts_pathlen;		/* strlen(fts_path) */
	short fts_namelen;		/* strlen(fts_name) */

#define	FTS_ROOTPARENTLEVEL	-1
#define	FTS_ROOTLEVEL		 0
	short fts_level;		/* depth (-1 to N) */

#define	FTS_D		 1		/* preorder directory */
#define	FTS_DC		 2		/* directory that causes cycles */
#define	FTS_DEFAULT	 3		/* none of the above */
#define	FTS_DNR		 4		/* unreadable directory */
#define	FTS_DP		 5		/* postorder directory */
#define	FTS_ERR		 6		/* error; errno is set */
#define	FTS_F		 7		/* regular file */
#define	FTS_NS		 8		/* stat(2) failed */
#define	FTS_NSOK	 9		/* no stat(2) requested */
#define	FTS_SL		10		/* symbolic link */
#define	FTS_SLNONE	11		/* symbolic link without target */
	u_short fts_info;		/* user flags for FTSENT structure */

#define	FTS_AGAIN	 1		/* read node again */
#define	FTS_FOLLOW	 2		/* follow symbolic link */
#define	FTS_NOINSTR	 3		/* no instructions */
#define	FTS_SKIP	 4		/* discard node */
	u_short fts_instr;		/* fts_set() instructions */

	struct stat fts_statb;		/* stat(2) information */
	char fts_name[1];		/* file name */
} FTSENT;

#include <sys/cdefs.h>

__BEGIN_DECLS
FTSENT	*fts_children __P((FTS *));
int	 fts_close __P((FTS *));
FTS	*fts_open __P((char * const *, int,
	    int (*)(const FTSENT **, const FTSENT **)));
FTSENT	*fts_read __P((FTS *));
int	 fts_set __P((FTS *, FTSENT *, int));
__END_DECLS

#endif /* !_FTS_H_ */
