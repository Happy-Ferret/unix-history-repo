/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)cmd7.c	3.8 (Berkeley) %G%";
#endif /* not lint */

#include "defs.h"
#include "string.h"

/*
 * Window size.
 */

c_size(w)
register struct ww *w;
{
	int col, row;

	if (!terse)
		wwputs("New window size (lower right corner): ", cmdwin);
	col = MIN(w->ww_w.r, wwncol) - 1;
	row = MIN(w->ww_w.b, wwnrow) - 1;
	wwadd(boxwin, framewin->ww_back);
	for (;;) {
		wwbox(boxwin, w->ww_w.t - 1, w->ww_w.l - 1,
			row - w->ww_w.t + 3, col - w->ww_w.l + 3);
		wwsetcursor(row, col);
		while (wwpeekc() < 0)
			wwiomux();
		switch (getpos(&row, &col, w->ww_w.t, w->ww_w.l,
			wwnrow - 1, wwncol - 1)) {
		case 3:
			wwunbox(boxwin);
			wwdelete(boxwin);
			return;
		case 2:
			wwunbox(boxwin);
			break;
		case 1:
			wwunbox(boxwin);
		case 0:
			continue;
		}
		break;
	}
	wwdelete(boxwin);
	if (!terse)
		wwputc('\n', cmdwin);
	wwcurtowin(cmdwin);
	sizewin(w, row - w->ww_w.t + 1, col - w->ww_w.l + 1);
}

/*
 * Yank and put
 */

struct yb {
	char *line;
	int length;
	struct yb *link;
};
struct yb *yb_head, *yb_tail;

c_yank()
{
	struct ww *w = selwin;
	int col1, row1;
	int col2, row2;
	int r, c;

	if (!terse)
		wwputs("Yank starting position: ", cmdwin);
	wwcursor(w, 0);
	row1 = w->ww_cur.r;
	col1 = w->ww_cur.c;
	for (;;) {
		wwsetcursor(row1, col1);
		while (wwpeekc() < 0)
			wwiomux();
		switch (getpos(&row1, &col1, w->ww_i.t, w->ww_i.l,
			       w->ww_i.b - 1, w->ww_i.r - 1)) {
		case 3:
			goto out;
		case 2:
			break;
		case 1:
		case 0:
			continue;
		}
		break;
	}
	if (!terse)
		wwputs("\nYank ending position: ", cmdwin);
	row2 = row1;
	col2 = col1;
	for (;;) {
		wwsetcursor(row2, col2);
		while (wwpeekc() < 0)
			wwiomux();
		r = row2;
		c = col2;
		switch (getpos(&row2, &col2, w->ww_i.t, w->ww_i.l,
			       w->ww_i.b - 1, w->ww_i.r - 1)) {
		case 3:
			yank_highlight(row1, col1, r, c);
			goto out;
		case 2:
			break;
		case 1:
			yank_highlight(row1, col1, r, c);
			yank_highlight(row1, col1, row2, col2);
		case 0:
			continue;
		}
		break;
	}
	if (row2 < row1 || row2 == row1 && col2 < col1) {
		r = row1;
		c = col1;
		row1 = row2;
		col1 = col2;
		row2 = r;
		col2 = c;
	}
	unyank();
	c = col1;
	for (r = row1; r < row2; r++) {
		yank_line(r, c, w->ww_b.r);
		c = w->ww_b.l;
	}
	yank_line(r, c, col2);
	yank_highlight(row1, col1, row2, col2);
	if (!terse)
		wwputc('\n', cmdwin);
out:
	wwcursor(w, 1);
}

yank_highlight(row1, col1, row2, col2)
{
	struct ww *w = selwin;
	int r, c;

	if ((wwavailmodes & WWM_REV) == 0)
		return;
	if (row2 < row1 || row2 == row1 && col2 < col1) {
		r = row1;
		c = col1;
		row1 = row2;
		col1 = col2;
		row2 = r;
		col2 = c;
	}
	c = col1;
	for (r = row1; r < row2; r++) {
		yank_highlight_line(r, c, w->ww_b.r);
		c = w->ww_b.l;
	}
	yank_highlight_line(r, c, col2);
}

yank_highlight_line(r, c, cend)
{
	struct ww *w = selwin;
	char *win;

	if (r < w->ww_i.t || r >= w->ww_i.b)
		return;
	if (c < w->ww_i.l)
		c = w->ww_i.l;
	if (cend >= w->ww_i.r)
		cend = w->ww_i.r;
	for (win = w->ww_win[r] + c; c < cend; c++, win++) {
		*win ^= WWM_REV;
		if (wwsmap[r][c] == w->ww_index) {
			if (*win == 0)
				w->ww_nvis[r]++;
			else if (*win == WWM_REV)
				w->ww_nvis[r]--;
			wwns[r][c].c_m ^= WWM_REV;
			wwtouched[r] |= WWU_TOUCHED;
		}
	}
}

unyank()
{
	struct yb *yp, *yq;

	for (yp = yb_head; yp; yp = yq) {
		yq = yp->link;
		str_free(yp->line);
		free((char *) yp);
	}
	yb_head = yb_tail = 0;
}

yank_line(r, c, cend)
{
	struct yb *yp;
	int nl = 0;
	int n;
	union ww_char *bp;
	char *cp;

	if (c == cend)
		return;
	if ((yp = (struct yb *) malloc(sizeof *yp)) == 0)
		return;
	yp->link = 0;
	nl = cend == selwin->ww_b.r;
	bp = selwin->ww_buf[r];
	for (cend--; cend >= c; cend--)
		if (bp[cend].c_c != ' ')
			break;
	yp->length = n = cend - c + 1;
	if (nl)
		yp->length++;
	yp->line = str_alloc(yp->length + 1);
	for (bp += c, cp = yp->line; --n >= 0;)
		*cp++ = bp++->c_c;
	if (nl)
		*cp++ = '\n';
	*cp = 0;
	if (yb_head)
		yb_tail = yb_tail->link = yp;
	else
		yb_head = yb_tail = yp;
}

c_put()
{
	struct yb *yp;

	for (yp = yb_head; yp; yp = yp->link)
		(void) write(selwin->ww_pty, yp->line, yp->length);
}
