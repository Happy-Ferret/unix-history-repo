/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)morg.c	8.1 (Berkeley) %G%";
#endif /* not lint */

# include	"monop.ext"

/*
 *	These routines deal with mortgaging.
 */

static char	*names[MAX_PRP+2],
		*morg_coms[]	= {
			"quit",		/*  0 */
			"print",	/*  1 */
			"where",	/*  2 */
			"own holdings",	/*  3 */
			"holdings",	/*  4 */
			"mortgage",	/*  5 */
			"unmortgage",	/*  6 */
			"buy",		/*  7 */
			"sell",		/*  8 */
			"card",		/*  9 */
			"pay",		/* 10 */
			"trade",	/* 11 */
			"resign",	/* 12 */
			"save game",	/* 13 */
			"restore game",	/* 14 */
			0
		};

static shrt	square[MAX_PRP+2];

static int	num_good,got_houses;

/*
 *	This routine is the command level response the mortgage command.
 * it gets the list of mortgageable property and asks which are to
 * be mortgaged.
 */
mortgage() {

	reg int	prop;

	for (;;) {
		if (set_mlist() == 0) {
			if (got_houses)
				printf("You can't mortgage property with houses on it.\n");
			else
				printf("You don't have any un-mortgaged property.\n");
			return;
		}
		if (num_good == 1) {
			printf("Your only mortageable property is %s\n",names[0]);
			if (getyn("Do you want to mortgage it? ") == 0)
				m(square[0]);
			return;
		}
		prop = getinp("Which property do you want to mortgage? ",names);
		if (prop == num_good)
			return;
		m(square[prop]);
		notify(cur_p);
	}
}
/*
 *	This routine sets up the list of mortgageable property
 */
set_mlist() {

	reg OWN	*op;

	num_good = 0;
	for (op = cur_p->own_list; op; op = op->next)
		if (!op->sqr->desc->morg)
			if (op->sqr->type == PRPTY && op->sqr->desc->houses)
				got_houses++;
			else {
				names[num_good] = op->sqr->name;
				square[num_good++] = sqnum(op->sqr);
			}
	names[num_good++] = "done";
	names[num_good--] = 0;
	return num_good;
}
/*
 *	This routine actually mortgages the property.
 */
m(prop)
reg int	prop; {

	reg int	price;

	price = board[prop].cost/2;
	board[prop].desc->morg = TRUE;
	printf("That got you $%d\n",price);
	cur_p->money += price;
}
/*
 *	This routine is the command level repsponse to the unmortgage
 * command.  It gets the list of mortgaged property and asks which are
 * to be unmortgaged.
 */
unmortgage() {

	reg int	prop;

	for (;;) {
		if (set_umlist() == 0) {
			printf("You don't have any mortgaged property.\n");
			return;
		}
		if (num_good == 1) {
			printf("Your only mortaged property is %s\n",names[0]);
			if (getyn("Do you want to unmortgage it? ") == 0)
				unm(square[0]);
			return;
		}
		prop = getinp("Which property do you want to unmortgage? ",names);
		if (prop == num_good)
			return;
		unm(square[prop]);
	}
}
/*
 *	This routine sets up the list of mortgaged property
 */
set_umlist() {

	reg OWN	*op;

	num_good = 0;
	for (op = cur_p->own_list; op; op = op->next)
		if (op->sqr->desc->morg) {
			names[num_good] = op->sqr->name;
			square[num_good++] = sqnum(op->sqr);
		}
	names[num_good++] = "done";
	names[num_good--] = 0;
	return num_good;
}
/*
 *	This routine actually unmortgages the property
 */
unm(prop)
reg int	prop; {

	reg int	price;

	price = board[prop].cost/2;
	board[prop].desc->morg = FALSE;
	price += price/10;
	printf("That cost you $%d\n",price);
	cur_p->money -= price;
	set_umlist();
}
/*
 *	This routine forces the indebted player to fix his
 * financial woes.
 */
force_morg() {

	told_em = fixing = TRUE;
	while (cur_p->money <= 0)
		fix_ex(getinp("How are you going to fix it up? ",morg_coms));
	fixing = FALSE;
}
/*
 *	This routine is a special execute for the force_morg routine
 */
fix_ex(com_num)
reg int	com_num; {

	told_em = FALSE;
	(*func[com_num])();
	notify();
}
