#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

$dtrace -ln 'syscall::*$1:entry' read | awk '{print $(NF-1),$NF}' | sort
$dtrace -ln 'syscall::$1*:entry' read | awk '{print $(NF-1),$NF}' | sort
$dtrace -ln 'syscall::re$1*:entry' ad | awk '{print $(NF-1),$NF}' | sort
$dtrace -ln 'syscall::$1l*:entry' read | awk '{print $(NF-1),$NF}' | sort
$dtrace -ln 'syscall::p$1[0-9][0-9]:entry' read | awk '{print $(NF-1),$NF}' | \
 sort

exit $status
