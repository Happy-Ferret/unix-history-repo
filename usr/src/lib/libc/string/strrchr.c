/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific written prior permission. This software
 * is provided ``as is'' without express or implied warranty.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)strrchr.c	5.3 (Berkeley) %G%";
#endif /* LIBC_SCCS and not lint */

#ifdef notdef
static char sccsid[] = "@(#)rindex.c	5.3 (Berkeley) 6/2/88";
#endif

#include <stdio.h>

char *
strrchr(p, ch)
	register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = p;
		if (!*p)
			return(save);
	}
	/* NOTREACHED */
}
