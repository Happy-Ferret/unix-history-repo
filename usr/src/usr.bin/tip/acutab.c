/*
 * Copyright (c) 1983 The Regents of the University of California.
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
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char sccsid[] = "@(#)acutab.c	5.3 (Berkeley) %G%";
#endif /* not lint */

#include "tip.h"

extern int df02_dialer(), df03_dialer(), df_disconnect(), df_abort(),
	   biz31f_dialer(), biz31_disconnect(), biz31_abort(),
	   biz31w_dialer(),
	   biz22f_dialer(), biz22_disconnect(), biz22_abort(),
	   biz22w_dialer(),
	   ven_dialer(), ven_disconnect(), ven_abort(),
	   hay_dialer(), hay_disconnect(), hay_abort(),
	   cour_dialer(), cour_disconnect(), cour_abort(),
	   v3451_dialer(), v3451_disconnect(), v3451_abort(),
	   v831_dialer(), v831_disconnect(), v831_abort(),
	   dn_dialer(), dn_disconnect(), dn_abort();

acu_t acutable[] = {
#if BIZ1031
	"biz31f", biz31f_dialer, biz31_disconnect,	biz31_abort,
	"biz31w", biz31w_dialer, biz31_disconnect,	biz31_abort,
#endif
#if BIZ1022
	"biz22f", biz22f_dialer, biz22_disconnect,	biz22_abort,
	"biz22w", biz22w_dialer, biz22_disconnect,	biz22_abort,
#endif
#if DF02
	"df02",	df02_dialer,	df_disconnect,		df_abort,
#endif
#if DF03
	"df03",	df03_dialer,	df_disconnect,		df_abort,
#endif
#if DN11
	"dn11",	dn_dialer,	dn_disconnect,		dn_abort,
#endif
#ifdef VENTEL
	"ventel",ven_dialer,	ven_disconnect,		ven_abort,
#endif
#ifdef HAYES
	"hayes",hay_dialer,	hay_disconnect,		hay_abort,
#endif
#ifdef COURIER
	"courier",cour_dialer,	cour_disconnect,	cour_abort,
#endif
#ifdef V3451
#ifndef V831
	"vadic",v3451_dialer,	v3451_disconnect,	v3451_abort,
#endif
	"v3451",v3451_dialer,	v3451_disconnect,	v3451_abort,
#endif
#ifdef V831
#ifndef V3451
	"vadic",v831_dialer,	v831_disconnect,	v831_abort,
#endif
	"v831",v831_dialer,	v831_disconnect,	v831_abort,
#endif
	0,	0,		0,			0
};

