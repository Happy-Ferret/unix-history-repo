/*-
 * Copyright (c) 1992, 1993, 1994 Henry Spencer.
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Henry Spencer of the University of Toronto.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)cclass.h	8.2 (Berkeley) %G%
 */

/* character-class table */
static struct cclass {
	char *name;
	char *chars;
	char *multis;
} cclasses[] = {
	"alnum",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789",				"",
	"alpha",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz",
					"",
	"blank",	" \t",		"",
	"cntrl",	"\007\b\t\n\v\f\r\1\2\3\4\5\6\16\17\20\21\22\23\24\
\25\26\27\30\31\32\33\34\35\36\37\177",	"",
	"digit",	"0123456789",	"",
	"graph",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
					"",
	"lower",	"abcdefghijklmnopqrstuvwxyz",
					"",
	"print",	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\
0123456789!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~ ",
					"",
	"punct",	"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
					"",
	"space",	"\t\n\v\f\r ",	"",
	"upper",	"ABCDEFGHIJKLMNOPQRSTUVWXYZ",
					"",
	"xdigit",	"0123456789ABCDEFabcdef",
					"",
	NULL,		0,		""
};
