/*
 *   Copyright (c) 1997 Hellmuth Michaelis. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	convert a-law / u-law sound files
 *	---------------------------------
 *
 *	last edit-date: [Sat Dec  5 17:57:56 1998]
 *
 * $FreeBSD$
 *
 *	-hm	telephony is ready
 *
 *---------------------------------------------------------------------------*/

#include <string.h>
#include <unistd.h>

#define BUF_SIZ	2048

unsigned char alaw_ulaw[];
unsigned char ulaw_alaw[];
	
int main(int argc, char *argv[])
{
	int i, j;
	unsigned char buffer[BUF_SIZ];
	char *p;
	unsigned char *cp;

	if((p = rindex(*argv, '/')) != NULL)
		p++;
	else
		p = *argv;

	if(!strcmp(p, "ulaw2alaw"))
	{
		cp = ulaw_alaw;
	}
	else if(!strcmp(p, "alaw2ulaw"))
	{
		cp = alaw_ulaw;
	}
	else
	{
		return(1);
	}
	
	while(((j = read(0, buffer, BUF_SIZ)) > 0))
	{
		for(i = 0; i < j; i++)
			buffer[i] = cp[buffer[i]];
		write(1, buffer, j);
	}
	return(0);
}

unsigned char alaw_ulaw[] = {
	0x002a, 0x00a9, 0x005f, 0x00e3, 0x001f, 0x009f, 0x0048, 0x00c8,
	0x0039, 0x00b9, 0x006f, 0x00f7, 0x001f, 0x009f, 0x0055, 0x00d7,
	0x0022, 0x00a1, 0x005b, 0x00dd, 0x001f, 0x009f, 0x0040, 0x00c0,
	0x0031, 0x00b1, 0x0067, 0x00eb, 0x001f, 0x009f, 0x004e, 0x00cf,
	0x002e, 0x00ad, 0x0063, 0x00e7, 0x001f, 0x009f, 0x004c, 0x00cc,
	0x003d, 0x00bd, 0x0077, 0x00ff, 0x001f, 0x009f, 0x0059, 0x00db,
	0x0026, 0x00a5, 0x005d, 0x00df, 0x001f, 0x009f, 0x0044, 0x00c4,
	0x0035, 0x00b5, 0x006b, 0x00ef, 0x001f, 0x009f, 0x0051, 0x00d3,
	0x0028, 0x00a7, 0x005f, 0x00e3, 0x001f, 0x009f, 0x0046, 0x00c6,
	0x0037, 0x00b7, 0x006f, 0x00f7, 0x001f, 0x009f, 0x0053, 0x00d5,
	0x0020, 0x009f, 0x005b, 0x00dd, 0x001f, 0x009f, 0x003f, 0x00bf,
	0x002f, 0x00af, 0x0067, 0x00eb, 0x001f, 0x009f, 0x004d, 0x00ce,
	0x002c, 0x00ab, 0x0063, 0x00e7, 0x001f, 0x009f, 0x004a, 0x00ca,
	0x003b, 0x00bb, 0x0077, 0x00ff, 0x001f, 0x009f, 0x0057, 0x00d9,
	0x0024, 0x00a3, 0x005d, 0x00df, 0x001f, 0x009f, 0x0042, 0x00c2,
	0x0033, 0x00b3, 0x006b, 0x00ef, 0x001f, 0x009f, 0x004f, 0x00d1,
	0x002b, 0x00aa, 0x0063, 0x00e3, 0x001f, 0x009f, 0x0049, 0x00c9,
	0x003a, 0x00ba, 0x0077, 0x00f7, 0x001f, 0x009f, 0x0057, 0x00d7,
	0x0023, 0x00a2, 0x005d, 0x00dd, 0x001f, 0x009f, 0x0041, 0x00c1,
	0x0032, 0x00b2, 0x006b, 0x00eb, 0x001f, 0x009f, 0x004f, 0x00cf,
	0x002f, 0x00ae, 0x0067, 0x00e7, 0x001f, 0x009f, 0x004d, 0x00cd,
	0x003e, 0x00be, 0x00ff, 0x00ff, 0x001f, 0x009f, 0x005b, 0x00db,
	0x0027, 0x00a6, 0x005f, 0x00df, 0x001f, 0x009f, 0x0045, 0x00c5,
	0x0036, 0x00b6, 0x006f, 0x00ef, 0x001f, 0x009f, 0x0053, 0x00d3,
	0x0029, 0x00a8, 0x005f, 0x00e3, 0x001f, 0x009f, 0x0047, 0x00c7,
	0x0038, 0x00b8, 0x006f, 0x00f7, 0x001f, 0x009f, 0x0055, 0x00d5,
	0x0021, 0x00a0, 0x005b, 0x00dd, 0x001f, 0x009f, 0x003f, 0x00bf,
	0x0030, 0x00b0, 0x0067, 0x00eb, 0x001f, 0x009f, 0x004e, 0x00ce,
	0x002d, 0x00ac, 0x0063, 0x00e7, 0x001f, 0x009f, 0x004b, 0x00cb,
	0x003c, 0x00bc, 0x0077, 0x00ff, 0x001f, 0x009f, 0x0059, 0x00d9,
	0x0025, 0x00a4, 0x005d, 0x00df, 0x001f, 0x009f, 0x0043, 0x00c3,
	0x0034, 0x00b4, 0x006b, 0x00ef, 0x001f, 0x009f, 0x0051, 0x00d1
};

unsigned char ulaw_alaw[] = {
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc,
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc,
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc,
	0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00fc, 0x00ac,
	0x0050, 0x00d0, 0x0010, 0x0090, 0x0070, 0x00f0, 0x0030, 0x00b0,
	0x0040, 0x00c0, 0x0000, 0x0080, 0x0060, 0x00e0, 0x0020, 0x00a0,
	0x00d8, 0x0018, 0x0098, 0x0078, 0x00f8, 0x0038, 0x00b8, 0x0048,
	0x00c8, 0x0008, 0x0088, 0x0068, 0x00e8, 0x0028, 0x00a8, 0x00d6,
	0x0096, 0x0076, 0x00f6, 0x0036, 0x00b6, 0x0046, 0x00c6, 0x0006,
	0x0086, 0x0066, 0x00e6, 0x0026, 0x00a6, 0x00de, 0x009e, 0x00fe,
	0x00fe, 0x00be, 0x00be, 0x00ce, 0x00ce, 0x008e, 0x008e, 0x00ee,
	0x00ee, 0x00d2, 0x00d2, 0x00f2, 0x00f2, 0x00c2, 0x00c2, 0x00e2,
	0x00e2, 0x00e2, 0x00da, 0x00da, 0x00da, 0x00da, 0x00fa, 0x00fa,
	0x00fa, 0x00fa, 0x00ca, 0x00ca, 0x00ca, 0x00ca, 0x00ea, 0x00ea,
	0x00ea, 0x00ea, 0x00ea, 0x00ea, 0x00eb, 0x00eb, 0x00eb, 0x00eb,
	0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb, 0x00eb,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd, 0x00fd,
	0x00d1, 0x0011, 0x0091, 0x0071, 0x00f1, 0x0031, 0x00b1, 0x0041,
	0x00c1, 0x0001, 0x0081, 0x0061, 0x00e1, 0x0021, 0x00a1, 0x0059,
	0x00d9, 0x0019, 0x0099, 0x0079, 0x00f9, 0x0039, 0x00b9, 0x0049,
	0x00c9, 0x0009, 0x0089, 0x0069, 0x00e9, 0x0029, 0x00a9, 0x0057,
	0x0017, 0x0097, 0x0077, 0x00f7, 0x0037, 0x00b7, 0x0047, 0x00c7,
	0x0007, 0x0087, 0x0067, 0x00e7, 0x0027, 0x00a7, 0x00df, 0x009f,
	0x009f, 0x00ff, 0x00ff, 0x00bf, 0x00bf, 0x00cf, 0x00cf, 0x008f,
	0x008f, 0x00ef, 0x00ef, 0x00af, 0x00af, 0x00d3, 0x00d3, 0x00f3,
	0x00f3, 0x00f3, 0x00c3, 0x00c3, 0x00c3, 0x00c3, 0x00e3, 0x00e3,
	0x00e3, 0x00e3, 0x00db, 0x00db, 0x00db, 0x00db, 0x00fb, 0x00fb,
	0x00fb, 0x00fb, 0x00fb, 0x00fb, 0x00cb, 0x00cb, 0x00cb, 0x00cb,
	0x00cb, 0x00cb, 0x00cb, 0x00cb, 0x00eb, 0x00eb, 0x00eb, 0x00eb
};

/* EOF */
