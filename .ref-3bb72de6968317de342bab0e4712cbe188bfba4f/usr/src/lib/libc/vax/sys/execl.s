/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifdef SYSLIBC_SCCS
_sccsid:.asciz	"@(#)execl.s	5.3 (Berkeley) %G%"
#endif SYSLIBC_SCCS

#include "SYS.h"

ENTRY(execl)
	pushab	8(ap)
	pushl	4(ap)
	calls	$2,_execv
	ret		# execl(file, arg1, arg2, ..., 0);
