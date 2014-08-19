#!/bin/sh
#
# $FreeBSD$
#
#################################################################
# Missing Features:
# It would be nice to have OIDs separated into composite groups
# using the subsection mdoc(7) feature (.Ss) without adding extra
# files.
#
# The ability to notice when new OIDs are added to FreeBSD, and
# and the automation of their sorting and addition into the
# tunables.mdoc file.
#
# Perhaps a re-implementation in C?  This wouldn't be much of
# a challenge for the right individual but it may require a lot
# of changes to sysctl.h.  Eventually this utility should replace
# documenting sysctls in code and manual pages since this utility
# creates a manual page dynamicly based on the kernel.  This
# would kill duplication between manual pages and kernel code as
# well as improve the removal of sysctls when they are obsoleted.
#################################################################

# Set our path up.
PATH=/bin:/usr/bin:/sbin:/usr/sbin

# Set a unique date format in the produced manual page.
DATE=`LC_TIME=C date +"%B %d, %Y"`

# We need a usage statement correct?
USAGE="Usage: run.sh -k [absolute path]"

# The endman function closes the list and adds the bottom
# part of our manual page.
endman() {
cat <<EOF>> ./sysctl.5
.El
.Sh IMPLEMENTATION NOTES
This manual page has been automatically generated by
a set of scripts written in
.Xr sh 1 .
The
.Xr mdoc 7
markup is stored in the database file and extracted
accordingly when invoked.
For information on the
.Xr sysctl 8
implementation, see the respecting manual pages.
.Sh SEE ALSO
.Xr loader.conf 5 ,
.Xr rc.conf 5 ,
.Xr sysctl.conf 5 ,
.Xr boot 8 ,
.Xr loader 8 ,
.Xr sysctl 8 ,
.Xr sysctl_add_oid 9 ,
.Xr sysctl_ctx_init 9
.Sh AUTHORS
This manual page is automatically generated
by a set of scripts written by
.An -nosplit
.An Tom Rhodes Aq Mt trhodes@FreeBSD.org ,
with significant contributions from
.An Giorgos Keramidas Aq Mt keramida@FreeBSD.org ,
.An Ruslan Ermilov Aq Mt ru@FreeBSD.org ,
and
.An Marc Silver Aq Mt marcs@draenor.org .
.Sh BUGS
Sometimes
.Fx
.Nm sysctls
can be left undocumented by those who originally
implemented them.
This script was forged as a way to automatically
produce a manual page to aid in the administration and
configuration of a
.Fx
system.
It also gets around adding a bunch of supporting code to the
.Nm
interface.
EOF
}

# The markup_create() function builds the actual
# markup file to be dropped into.  In essence,
# compare our list of tunables with the documented
# tunables in our tunables.mdoc file and generate
# the final 'inner circle' of our manual page.
markup_create() {
	sort -u  < _names |		\
	xargs -n 1 /bin/sh ./sysctl.sh  \
		> markup.file		\
		2> tunables.TODO
	rm _names
}

# Finally, the following lines will call our functions and
# and create our document using the following function:
page_create() {
	startman
	/bin/cat ./markup.file >> sysctl.5
	endman
}

# The startman function creates the initial mdoc(7) formatted
# manual page.  This is required before we populate it with
# tunables both loader and sysctl(8) oids.
startman() {
cat <<EOF>> ./sysctl.5
.\"
.\" Copyright (c) 2005 Tom Rhodes
.\" All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistribution of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistribution's in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\"
.Dd $DATE
.Dt SYSCTL 5
.Os
.Sh NAME
.Nm sysctl
.Nd "list of available syctls based on kernel configuration"
.Sh DESCRIPTION
.Fx
supports kernel alterations on the fly or at
system initialization by using a feature
known as a
.Nm
and a database.
Many values may be altered simply by using the
.Xr sysctl 8
utility followed by a
.Nm
and its new value at the command prompt.
For example:
.Dl sysctl kern.ipc.zero_copy.receive=1
will enable zero copy sockets for receive.
.Pp
Many variables may only be available if specific kernel
options are built into the kernel.
For example, the previous
.Nm
requires
.Xr zero_copy 9 .
.Pp
Most of these values only offer an enable/disable
option, altered by using a numerical value of
.Dq 0
or
.Dq 1
where the former is disable and latter is enable.
Other cases the
.Nm
may require a string value.
The
.Xr sysctl 8
utility may be used to dump a list of current
values which should provide an example of
the non-numeric cases.
.Pp
In cases where the value may not be altered, the
following warning will be issued:
.Dq read only value
and the
.Nm
will not be changed.
To alter these values, the administrator may place
them in the
.Xr sysctl.conf 5
file.
This will invoke the changes during
system initialization for those values
which may be altered.
In other cases, the
.Xr loader.conf 5
may be used.
Then again, some of these
.Nm sysctls
may never be altered.
.Pp
The
.Nm
supported by
.Xr sysctl 8
are:
.Pp
.Bl -ohang -offset indent
EOF
}

#
# The nm(1) utility must only be used on the architecture which
# we build it for.  Although i386 and pc98 are so; my only fear
# with this is that this will not work properly on cross-builds.

while getopts k FLAG;
  do
    case "$FLAG" in

	k)  LOCATION="$OPTARG" ;;

	*)  echo "$USAGE"
	    exit 0 ;;

  esac
done

# The k flag
shift

if [ -z "$1" ] && [ -z "$LOCATION" ] ;
  then echo "Malformed or improper path specified";
  exit 1;
fi

if [ -z "$LOCATION" ] ;
  then LOCATION="$1" \
    && for x in `find $LOCATION -name '*.kld'`  \
	$LOCATION/kernel;			\
	do nm $x |				\
	sed -n '/sysctl___/ {
		's/[\.a-z_]*sysctl___//g'
		's/_/./g'
		p
	}' |					\
	awk {'print $3'} |			\
	sort -u > _names;
	done;
	markup_create
	page_create
fi
