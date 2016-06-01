/*	$NetBSD: internal.h,v 1.14 2016/04/04 15:52:56 christos Exp $	*/

/*-
 * Copyright (c) 2015 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _INTERNAL_H
#define _INTERNAL_H

#ifndef _PATH_BLCONF
#define	_PATH_BLCONF	"/etc/blacklistd.conf"
#endif
#ifndef _PATH_BLCONTROL
#define	_PATH_BLCONTROL	"/libexec/blacklistd-helper"
#endif
#ifndef _PATH_BLSTATE
#define	_PATH_BLSTATE	"/var/db/blacklistd.db"
#endif

extern struct confset rconf, lconf;
extern int debug;
extern const char *rulename;
extern const char *controlprog;
extern struct ifaddrs *ifas;

#if !defined(__syslog_attribute__) && !defined(__syslog__)
#define __syslog__ __printf__
#endif

extern void (*lfun)(int, const char *, ...)
    __attribute__((__format__(__syslog__, 2, 3)));

#endif /* _INTERNAL_H */
