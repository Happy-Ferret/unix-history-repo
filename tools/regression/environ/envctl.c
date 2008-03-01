/*-
 * Copyright (c) 2007 Sean C. Farley <scf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


extern char **environ;


/*
 * Print entire environ array.
 */
static void
dump_environ(void)
{
	char **environPtr;

	for (environPtr = environ; *environPtr != NULL; environPtr++)
		printf("%s\n", *environPtr);

	return;
}


/*
 * Print usage.
 */
static void
usage(const char *program)
{
	fprintf(stderr, "Usage:  %s [-CDGUchrt] [-gu name] [-p name=value] "
	    "[(-S|-s name) value overwrite]\n\n"
	    "Options:\n"
	    "  -C\t\t\t\tClear environ variable with NULL pointer\n"
	    "  -D\t\t\t\tDump environ\n"
	    "  -G name\t\t\tgetenv(NULL)\n"
	    "  -S value overwrite\t\tsetenv(NULL, value, overwrite)\n"
	    "  -U\t\t\t\tunsetenv(NULL)\n"
	    "  -c\t\t\t\tClear environ variable with calloc()'d memory\n"
	    "  -g name\t\t\tgetenv(name)\n"
	    "  -h\t\t\t\tHelp\n"
	    "  -p name=value\t\t\tputenv(name=value)\n"
	    "  -r\t\t\t\treplace environ with { \"FOO=bar\", NULL }\n"
	    "  -s name value overwrite\tsetenv(name, value, overwrite)\n"
	    "  -t\t\t\t\tOutput is suitable for testing (no newlines)\n"
	    "  -u name\t\t\tunsetenv(name)\n",
	    basename(program));

	return;
}


/*
 * Print the return value of a call along with errno upon error else zero.
 * Also, use the eol string based upon whether running in test mode or not.
 */
static void
print_rtrn_errno(int rtrnVal, const char *eol)
{
	printf("%d %d%s", rtrnVal, rtrnVal != 0 ? errno : 0, eol);

	return;
}


int
main(int argc, char **argv)
{
	char *staticEnv[] = { "FOO=bar", NULL };
	char arg;
	const char *eol = "\n";
	const char *value;

	if (argc == 1) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* The entire program is basically executed from this loop. */
	while ((arg = getopt(argc, argv, "CDGS:Ucg:hp:rs:tu:")) != -1) {
		switch (arg) {
			case 'C':
				environ = NULL;
				break;

			case 'c':
				environ = calloc(1, sizeof(*environ));
				break;

			case 'D':
				dump_environ();
				break;

			case 'G':
			case 'g':
				value = getenv(arg == 'g' ? optarg : NULL);
				printf("%s%s", value == NULL ? "" : value, eol);
				break;

			case 'p':
				print_rtrn_errno(putenv(optarg), eol);
				break;

			case 'r':
				environ = staticEnv;
				break;

			case 'S':
				print_rtrn_errno(setenv(NULL, optarg,
				    atoi(argv[optind])), eol);
				optind += 1;
				break;

			case 's':
				print_rtrn_errno(setenv(optarg, argv[optind],
				    atoi(argv[optind + 1])), eol);
				optind += 2;
				break;

			case 't':
				eol = " ";
				break;

			case 'U':
			case 'u':
				print_rtrn_errno(unsetenv(arg == 'u' ? optarg :
				    NULL), eol);
				break;

			case 'h':
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	/* Output a closing newline in test mode. */
	if (eol[0] == ' ')
		printf("\n");

	return (EXIT_SUCCESS);
}
