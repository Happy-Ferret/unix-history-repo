#ifndef lint
static	char *sccsid = "@(#)wwclreos.c	3.3 83/09/15";
#endif

#include "ww.h"

wwclreos(w, row, col)
register struct ww *w;
{
	register i;

	wwclreol(w, row, col);
	for (i = row + 1; i < w->ww_b.b; i++)
		wwclreol(w, i, w->ww_b.l);
}
