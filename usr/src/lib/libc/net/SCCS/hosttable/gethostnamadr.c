/*	gethostnamadr.c	4.3	83/12/21	*/

#include <stdio.h>
#include <netdb.h>
#include <sys/file.h>
#include <ndbm.h>

#define	MAXALIASES	35

static char HOSTDB[] = "/etc/hosts";
DBM *_host_db = (DBM *)NULL;
static datum curkey;
static struct hostent host;
static char *host_aliases[MAXALIASES];
int _host_stayopen;	/* set by sethostent(), cleared by endhostent() */

static struct hostent *
fetchhost(key)
	datum key;
{
        register char *cp, **ap;
	register int naliases;

        curkey = key;
        if (curkey.dptr == 0)
                return ((struct hostent *)NULL);
	key = dbmfetch(_host_db, curkey);
	if (key.dptr == 0)
                return ((struct hostent *)NULL);
        cp = key.dptr;
	host.h_name = cp;
	while (*cp++)
		;
	naliases = *(int *)cp; cp += sizeof (int);
	for (ap = host_aliases; naliases > 0; naliases--) {
		*ap++ = cp;
		while (*cp++)
			;
	}
	*ap = (char *)NULL;
	host.h_aliases = host_aliases;
	host.h_addrtype = *(int *)cp; cp += sizeof (int);
	host.h_length = *(int *)cp; cp += sizeof (int);
	host.h_addr = cp;
        return (&host);
}

struct hostent *
gethostbyname(nam)
	char *nam;
{
        datum key;
	register struct hostent *hp;

	if ((_host_db == (DBM *)NULL)
	  && ((_host_db = ndbmopen(HOSTDB, O_RDONLY)) == (DBM *)NULL))
                return ((struct hostent *)NULL);
        key.dptr = nam;
        key.dsize = strlen(nam);
	hp = fetchhost(key);
	if (!_host_stayopen) {
		ndbmclose(_host_db);
		_host_db = (DBM *)NULL;
	}
        return (hp);
}

struct hostent *
gethostbyaddr(addr, length)
	char *addr;
	int length;
{
        datum key;
	register struct hostent *hp;

	if ((_host_db == (DBM *)NULL)
	  && ((_host_db = ndbmopen(HOSTDB, O_RDONLY)) == (DBM *)NULL))
                return ((struct hostent *)NULL);
        key.dptr = addr;
        key.dsize = length;
	hp = fetchhost(key);
	if (!_host_stayopen) {
		ndbmclose(_host_db);
		_host_db = (DBM *)NULL;
	}
        return (hp);
}
