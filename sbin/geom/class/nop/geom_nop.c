/*-
 * Copyright (c) 2004 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdint.h>
#include <libgeom.h>
#include <geom/nop/g_nop.h>

#include "core/geom.h"


uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_NOP_VERSION;

static intmax_t failprob = 0;
static intmax_t offset = 0;
static intmax_t size = 0;

struct g_command class_commands[] = {
	{ "create", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
	    {
		{ 'f', "failprob", &failprob, G_TYPE_NUMBER },
		{ 'o', "offset", &offset, G_TYPE_NUMBER },
		{ 's', "size", &size, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    }
	},
	{ "configure", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "failprob", &failprob, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    }
	},
	{ "destroy", G_FLAG_VERBOSE, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    }
	},
	G_CMD_SENTINEL
};

void usage(const char *name);
void
usage(const char *name)
{

	fprintf(stderr, "usage: %s create [-v] [-f failprob] [-o offset] [-s size] <dev1> [dev2 [...]]\n", name);
	fprintf(stderr, "       %s configure [-v] [-f failprob] <prov1> [prov2 [...]]\n", name);
	fprintf(stderr, "       %s destroy [-fv] <prov1> [prov2 [...]]\n", name);
}
