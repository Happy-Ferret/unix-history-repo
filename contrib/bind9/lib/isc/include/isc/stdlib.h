/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: stdlib.h,v 1.2.18.2 2005-04-29 00:17:03 marka Exp $ */

#ifndef ISC_STDLIB_H
#define ISC_STDLIB_H 1

/*! \file */

#include <stdlib.h>

#include <isc/lang.h>
#include <isc/platform.h>

#ifdef ISC_PLATFORM_NEEDSTRTOUL
#define strtoul isc_strtoul
#endif

ISC_LANG_BEGINDECLS

unsigned long isc_strtoul(const char *, char **, int);

ISC_LANG_ENDDECLS

#endif
