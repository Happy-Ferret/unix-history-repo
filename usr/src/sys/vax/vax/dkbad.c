/*	dkbad.c	6.2	84/08/28	*/

#ifndef NOBADSECT
#include "param.h"
#include "buf.h"
#include "dkbad.h"

/*
 * Search the bad sector table looking for
 * the specified sector.  Return index if found.
 * Return -1 if not found.
 */

isbad(bt, cyl, trk, sec)
	register struct dkbad *bt;
{
	register int i;
	register long blk, bblk;

	blk = ((long)cyl << 16) + (trk << 8) + sec;
	for (i = 0; i < 126; i++) {
		bblk = ((long)bt->bt_bad[i].bt_cyl << 16) + bt->bt_bad[i].bt_trksec;
		if (blk == bblk)
			return (i);
		if (blk < bblk || bblk < 0)
			break;
	}
	return (-1);
}
#endif
