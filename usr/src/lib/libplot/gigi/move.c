/*-
 * Copyright (c) 1980 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.proprietary.c%
 */

#ifndef lint
static char sccsid[] = "@(#)move.c	5.2 (Berkeley) %G%";
#endif /* not lint */

#include "gigi.h"

move(xi,yi)
int xi,yi;
{
	currentx = xsc(xi);
	currenty = ysc(yi);
	printf("P[%d,%d]",currentx, currenty);
}
