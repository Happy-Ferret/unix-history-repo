/*
 * Copyright (c) 1996, 1997 Shigio Yamaguchi. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Shigio Yamaguchi.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	tag.c					20-Oct-97
 *
 */
#include <ctype.h>
#include <string.h>

#include "dbio.h"
#include "die.h"
#include "gtagsopen.h"
#include "locatestring.h"
#include "tab.h"
#include "tag.h"


static DBIO	*dbio;
static int	opened;

void
tagopen(dbpath, db, mode)
char	*dbpath;
int	db;
int	mode;
{
	if (opened)
		die("nested call to tagopen.");
	opened = 1;
	dbio = gtagsopen(dbpath, db, mode);
}
void
tagput(entry, record)
char	*entry;
char	*record;
{
	entab(record);
	db_put(dbio, entry, record);
}
void
tagdelete(path)
char	*path;
{
	char	*p;
	int	length = strlen(path);

	for (p = db_first(dbio, NULL, DBIO_SKIPMETA); p; p = db_next(dbio)) {
		p = locatestring(p, "./", 0);
		if (!strncmp(p, path, length) && isspace(*(p + length)))
			db_del(dbio, NULL);
	}
}
void
tagclose(void)
{
	db_close(dbio);
	opened = 0;
}
