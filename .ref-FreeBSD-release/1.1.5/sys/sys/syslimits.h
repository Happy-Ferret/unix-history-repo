/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)syslimits.h	7.4 (Berkeley) 2/4/91
 *	$Id: syslimits.h,v 1.3 1993/11/07 17:53:05 wollman Exp $
 */

#ifndef _SYS_SYSLIMITS_H_
#define _SYS_SYSLIMITS_H_ 1

#define	ARG_MAX		32768	/* max bytes for an exec function */
#define	CHILD_MAX	40	/* max simultaneous processes */
#define	LINK_MAX	32767	/* max file link count */
#define	MAX_CANON	255	/* max bytes in terminal canonical input line */
#define	MAX_INPUT	255	/* max bytes in terminal input */
#define	NAME_MAX	255	/* max number of bytes in a file name */
#define	NGROUPS_MAX	16	/* max number of supplemental group id's */
#define	OPEN_MAX	64	/* max open files per process */
#define	PATH_MAX	1024	/* max number of bytes in pathname */
#define	PIPE_BUF	512	/* max number of bytes for atomic pipe writes */

#define	BC_BASE_MAX	99	/* max ibase/obase values allowed by bc(1) */
#define	BC_DIM_MAX	2048	/* max array elements allowed by bc(1) */
#define	BC_SCALE_MAX	99	/* max scale value allowed by bc(1) */
#define	BC_STRING_MAX	1000	/* max const string length allowed by bc(1) */
#define	EQUIV_CLASS_MAX	2	/* max weights for order keyword; see locale */
#define	EXPR_NEST_MAX	32	/* max expressions nested in expr(1) */
#define	LINE_MAX	2048	/* max length in bytes of an input line */
#define	RE_DUP_MAX	255	/* max repeated RE's using interval notation */
#endif /* _SYS_SYSLIMITS_H_ */
