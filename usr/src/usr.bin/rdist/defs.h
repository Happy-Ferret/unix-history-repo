/*	defs.h	4.12	84/04/06	*/

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>

/*
 * The version number should be changed whenever the protocol changes.
 */
#define VERSION	 2

#define	MAILCMD	 "/usr/lib/sendmail -oi -t"

	/* defines for yacc */
#define EQUAL	1
#define LP	2
#define RP	3
#define SM	4
#define ARROW	5
#define COLON	6
#define DCOLON	7
#define NAME	8
#define STRING	9
#define INSTALL	10
#define NOTIFY	11
#define EXCEPT	12
#define SPECIAL	13
#define OPTION	14

	/* lexical definitions */
#define	QUOTE 	0200		/* used internally for quoted characters */
#define	TRIM	0177		/* Mask to strip quote bit */

	/* table sizes */
#define HASHSIZE	1021
#define INMAX	3500
#define NCARGS	10240
#define GAVSIZ	NCARGS / 6

	/* option flags */
#define VERIFY	0x1
#define WHOLE	0x2
#define YOUNGER	0x4
#define COMPARE	0x8
#define REMOVE	0x10

	/* expand type definitions */
#define E_VARS	0x1
#define E_SHELL	0x2
#define E_TILDE	0x4
#define E_ALL	0x7

	/* actions for lookup() */
#define LOOKUP	0
#define INSERT	1
#define REPLACE	2

#define ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

#define ALLOC(x) (struct x *) malloc(sizeof(struct x))

struct namelist {	/* for making lists of strings */
	char	*n_name;
	struct	namelist *n_next;
};

struct subcmd {
	short	sc_type;	/* type - INSTALL,NOTIFY,EXCEPT,SPECIAL */
	short	sc_options;
	char	*sc_name;
	struct	namelist *sc_args;
	struct	subcmd *sc_next;
};

struct cmd {
	int	c_type;		/* type - ARROW,DCOLON */
	char	*c_name;	/* hostname or time stamp file name */
	char	*c_label;	/* label for partial update */
	struct	namelist *c_files;
	struct	subcmd *c_cmds;
	struct	cmd *c_next;
};
	
extern int debug;		/* debugging flag */
extern int nflag;		/* NOP flag, don't execute commands */
extern int qflag;		/* Quiet. don't print messages */
extern int options;		/* global options */

extern int nerrs;		/* number of errors seen */
extern int rem;			/* remote file descriptor */
extern int iamremote;		/* acting as remote server */
extern char tmpfile[];		/* file name for logging changes */
extern struct passwd *pw;	/* pointer to static area used by getpwent */
extern struct group *gr;	/* pointer to static area used by getgrent */
extern char host[];		/* host name of master copy */
extern struct namelist *except;	/* list of files to exclude */
extern char buf[];		/* general purpose buffer */
extern int errno;		/* system error number */
extern char *sys_errlist[];

char *makestr();
struct namelist *makenl();
struct subcmd *makesubcmd();
struct namelist *lookup();
struct namelist *expand();
char *exptilde();
char *malloc();
char *rindex();
char *index();
