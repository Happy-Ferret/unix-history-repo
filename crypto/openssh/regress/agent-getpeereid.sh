#	$OpenBSD: agent-getpeereid.sh,v 1.3 2006/07/06 12:01:53 grunk Exp $
#	Placed in the Public Domain.

tid="disallow agent attach from other uid"

UNPRIV=nobody
ASOCK=${OBJ}/agent
SSH_AUTH_SOCK=/nonexistant

if grep "#undef.*HAVE_GETPEEREID" ${BUILDDIR}/config.h >/dev/null 2>&1
then
	echo "skipped (not supported on this platform)"
	exit 0
fi
if [ -z "$SUDO" ]; then
	echo "skipped: need SUDO to switch to uid $UNPRIV"
	exit 0
fi


trace "start agent"
eval `${SSHAGENT} -s -a ${ASOCK}` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fail "could not start ssh-agent: exit code $r"
else
	chmod 644 ${SSH_AUTH_SOCK}

	ssh-add -l > /dev/null 2>&1
	r=$?
	if [ $r -ne 1 ]; then
		fail "ssh-add failed with $r != 1"
	fi

	< /dev/null ${SUDO} -S -u ${UNPRIV} ssh-add -l > /dev/null 2>&1
	r=$?
	if [ $r -lt 2 ]; then
		fail "ssh-add did not fail for ${UNPRIV}: $r < 2"
	fi

	trace "kill agent"
	${SSHAGENT} -k > /dev/null
fi

rm -f ${OBJ}/agent
