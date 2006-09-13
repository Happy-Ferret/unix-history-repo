/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Test that privilege is required to write to the system extended attribute
 * namespace.
 */

#include <sys/types.h>
#include <sys/extattr.h>

#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "main.h"

#define	EA_NAMESPACE	EXTATTR_NAMESPACE_SYSTEM
#define	EA_NAME		"test"
#define	EA_DATA		"test"
#define	EA_SIZE		strlen(EA_DATA)

void
priv_vfs_extattr_system(void)
{
	char fpath[1024];
	int error;

	assert_root();

	/*
	 * Set file perms so that discretionary access control would grant
	 * write rights on non-system EAs on the file.
	 */
	setup_file(fpath, UID_ROOT, GID_WHEEL, 0666);

	/*
	 * Try with privilege.
	 */
	if (extattr_set_file(fpath, EA_NAMESPACE, EA_NAME, EA_DATA, EA_SIZE)
	    < 0) {
		warn("extattr_set_file(SYSTEM, %s, %s, %d) as root",
		    EA_NAME, EA_DATA, EA_SIZE);
		goto out;
	}

	set_euid(UID_OTHER);

	/*
	 * Try without privilege.
	 */
	error = extattr_set_file(fpath, EA_NAMESPACE, EA_NAME, EA_DATA,
	   EA_SIZE);
	if (error == 0) {
		warn("extattr_set_file(SYSTEM, %s, %s, %d) succeeded as !root",
		    EA_NAME, EA_DATA, EA_SIZE);
		goto out;
	}
	if (errno != EPERM) {
		warn("extattr_set_file(SYSTEM, %s, %s, %d) wrong errno %d "
		    "as !root", EA_NAME, EA_DATA, EA_SIZE, errno);
		goto out;
	}
out:
	seteuid(UID_ROOT);
	(void)unlink(fpath);
}
