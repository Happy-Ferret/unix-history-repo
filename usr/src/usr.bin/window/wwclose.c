#ifndef lint
static	char *sccsid = "@(#)wwclose.c	3.7 83/12/01";
#endif

#include "ww.h"
#include <signal.h>

wwclose(w)
register struct ww *w;
{
	wwindex[w->ww_index] = 0;
	if (w->ww_state == WWS_HASPROC)
		(void) kill(w->ww_pid, SIGHUP);
	(void) close(w->ww_pty);
	wwfree((char **)w->ww_win, w->ww_w.t);
	wwfree((char **)w->ww_buf, w->ww_w.t);
	if (w->ww_fmap != 0)
		wwfree((char **)w->ww_fmap, w->ww_w.t);
	free((char *)(w->ww_nvis + w->ww_w.t));
	free((char *)w);
}
