/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char sccsid[] = "@(#)phaser.c	5.1 (Berkeley) 1/29/86";
#endif not lint

# include	"trek.h"
# include	"getpar.h"

/* factors for phaser hits; see description below */

# define	ALPHA		3.0		/* spread */
# define	BETA		3.0		/* franf() */
# define	GAMMA		0.30		/* cos(angle) */
# define	EPSILON		150.0		/* dist ** 2 */
# define	OMEGA		10.596		/* overall scaling factor */

/* OMEGA ~= 100 * (ALPHA + 1) * (BETA + 1) / (EPSILON + 1) */

/*
**  Phaser Control
**
**	There are up to NBANKS phaser banks which may be fired
**	simultaneously.  There are two modes, "manual" and
**	"automatic".  In manual mode, you specify exactly which
**	direction you want each bank to be aimed, the number
**	of units to fire, and the spread angle.  In automatic
**	mode, you give only the total number of units to fire.
**
**	The spread is specified as a number between zero and
**	one, with zero being minimum spread and one being maximum
**	spread.  You  will normally want zero spread, unless your
**	short range scanners are out, in which case you probably
**	don't know exactly where the Klingons are.  In that case,
**	you really don't have any choice except to specify a
**	fairly large spread.
**
**	Phasers spread slightly, even if you specify zero spread.
**
**	Uses trace flag 30
*/

struct cvntab	Matab[] =
{
	"m",		"anual",		(int (*)())1,		0,
	"a",		"utomatic",		0,		0,
	0
};

struct banks
{
	int	units;
	double	angle;
	double	spread;
};



phaser()
{
	register int		i;
	int			j;
	register struct kling	*k;
	double			dx, dy;
	double			anglefactor, distfactor;
	register struct banks	*b;
	int			manual, flag, extra;
	int			hit;
	double			tot;
	int			n;
	int			hitreqd[NBANKS];
	struct banks		bank[NBANKS];
	struct cvntab		*ptr;

	if (Ship.cond == DOCKED)
		return(printf("Phasers cannot fire through starbase shields\n"));
	if (damaged(PHASER))
		return (out(PHASER));
	if (Ship.shldup)
		return (printf("Sulu: Captain, we cannot fire through shields.\n"));
	if (Ship.cloaked)
	{
		printf("Sulu: Captain, surely you must realize that we cannot fire\n");
		printf("  phasers with the cloaking device up.\n");
		return;
	}

	/* decide if we want manual or automatic mode */
	manual = 0;
	if (testnl())
	{
		if (damaged(COMPUTER))
		{
			printf(Device[COMPUTER].name);
			manual++;
		}
		else
			if (damaged(SRSCAN))
			{
				printf(Device[SRSCAN].name);
				manual++;
			}
		if (manual)
			printf(" damaged, manual mode selected\n");
	}

	if (!manual)
	{
		ptr = getcodpar("Manual or automatic", Matab);
		manual = (int) ptr->value;
	}
	if (!manual && damaged(COMPUTER))
	{
		printf("Computer damaged, manual selected\n");
		skiptonl(0);
		manual++;
	}

	/* initialize the bank[] array */
	flag = 1;
	for (i = 0; i < NBANKS; i++)
		bank[i].units = 0;
	if (manual)
	{
		/* collect manual mode statistics */
		while (flag)
		{
			printf("%d units available\n", Ship.energy);
			extra = 0;
			flag = 0;
			for (i = 0; i < NBANKS; i++)
			{
				b = &bank[i];
				printf("\nBank %d:\n", i);
				hit = getintpar("units");
				if (hit < 0)
					return;
				if (hit == 0)
					break;
				extra += hit;
				if (extra > Ship.energy)
				{
					printf("available energy exceeded.  ");
					skiptonl(0);
					flag++;
					break;
				}
				b->units = hit;
				hit = getintpar("course");
				if (hit < 0 || hit > 360)
					return;
				b->angle = hit * 0.0174532925;
				b->spread = getfltpar("spread");
				if (b->spread < 0 || b->spread > 1)
					return;
			}
			Ship.energy -= extra;
		}
		extra = 0;
	}
	else
	{
		/* automatic distribution of power */
		if (Etc.nkling <= 0)
			return (printf("Sulu: But there are no Klingons in this quadrant\n"));
		printf("Phasers locked on target.  ");
		while (flag)
		{
			printf("%d units available\n", Ship.energy);
			hit = getintpar("Units to fire");
			if (hit <= 0)
				return;
			if (hit > Ship.energy)
			{
				printf("available energy exceeded.  ");
				skiptonl(0);
				continue;
			}
			flag = 0;
			Ship.energy -= hit;
			extra = hit;
			n = Etc.nkling;
			if (n > NBANKS)
				n = NBANKS;
			tot = n * (n + 1) / 2;
			for (i = 0; i < n; i++)
			{
				k = &Etc.klingon[i];
				b = &bank[i];
				distfactor = k->dist;
				anglefactor = ALPHA * BETA * OMEGA / (distfactor * distfactor + EPSILON);
				anglefactor *= GAMMA;
				distfactor = k->power;
				distfactor /= anglefactor;
				hitreqd[i] = distfactor + 0.5;
				dx = Ship.sectx - k->x;
				dy = k->y - Ship.secty;
				b->angle = atan2(dy, dx);
				b->spread = 0.0;
				b->units = ((n - i) / tot) * extra;
#				ifdef xTRACE
				if (Trace)
				{
					printf("b%d hr%d u%d df%.2f af%.2f\n",
						i, hitreqd[i], b->units,
						distfactor, anglefactor);
				}
#				endif
				extra -= b->units;
				hit = b->units - hitreqd[i];
				if (hit > 0)
				{
					extra += hit;
					b->units -= hit;
				}
			}

			/* give out any extra energy we might have around */
			if (extra > 0)
			{
				for (i = 0; i < n; i++)
				{
					b = &bank[i];
					hit = hitreqd[i] - b->units;
					if (hit <= 0)
						continue;
					if (hit >= extra)
					{
						b->units += extra;
						extra = 0;
						break;
					}
					b->units = hitreqd[i];
					extra -= hit;
				}
				if (extra > 0)
					printf("%d units overkill\n", extra);
			}
		}
	}

#	ifdef xTRACE
	if (Trace)
	{
		for (i = 0; i < NBANKS; i++)
		{
			b = &bank[i];
			printf("b%d u%d", i, b->units);
			if (b->units > 0)
				printf(" a%.2f s%.2f\n", b->angle, b->spread);
			else
				printf("\n");
		}
	}
#	endif

	/* actually fire the shots */
	Move.free = 0;
	for (i = 0; i < NBANKS; i++)
	{
		b = &bank[i];
		if (b->units <= 0)
		{
			continue;
		}
		printf("\nPhaser bank %d fires:\n", i);
		n = Etc.nkling;
		k = Etc.klingon;
		for (j = 0; j < n; j++)
		{
			if (b->units <= 0)
				break;
			/*
			** The formula for hit is as follows:
			**
			**  zap = OMEGA * [(sigma + ALPHA) * (rho + BETA)]
			**	/ (dist ** 2 + EPSILON)]
			**	* [cos(delta * sigma) + GAMMA]
			**	* hit
			**
			** where sigma is the spread factor,
			** rho is a random number (0 -> 1),
			** GAMMA is a crud factor for angle (essentially
			**	cruds up the spread factor),
			** delta is the difference in radians between the
			**	angle you are shooting at and the actual
			**	angle of the klingon,
			** ALPHA scales down the significance of sigma,
			** BETA scales down the significance of rho,
			** OMEGA is the magic number which makes everything
			**	up to "* hit" between zero and one,
			** dist is the distance to the klingon
			** hit is the number of units in the bank, and
			** zap is the amount of the actual hit.
			**
			** Everything up through dist squared should maximize
			** at 1.0, so that the distance factor is never
			** greater than one.  Conveniently, cos() is
			** never greater than one, but the same restric-
			** tion applies.
			*/
			distfactor = BETA + franf();
			distfactor *= ALPHA + b->spread;
			distfactor *= OMEGA;
			anglefactor = k->dist;
			distfactor /= anglefactor * anglefactor + EPSILON;
			distfactor *= b->units;
			dx = Ship.sectx - k->x;
			dy = k->y - Ship.secty;
			anglefactor = atan2(dy, dx) - b->angle;
			anglefactor = cos((anglefactor * b->spread) + GAMMA);
			if (anglefactor < 0.0)
			{
				k++;
				continue;
			}
			hit = anglefactor * distfactor + 0.5;
			k->power -= hit;
			printf("%d unit hit on Klingon", hit);
			if (!damaged(SRSCAN))
				printf(" at %d,%d", k->x, k->y);
			printf("\n");
			b->units -= hit;
			if (k->power <= 0)
			{
				killk(k->x, k->y);
				continue;
			}
			k++;
		}
	}

	/* compute overkill */
	for (i = 0; i < NBANKS; i++)
		extra += bank[i].units;
	if (extra > 0)
		printf("\n%d units expended on empty space\n", extra);
}
