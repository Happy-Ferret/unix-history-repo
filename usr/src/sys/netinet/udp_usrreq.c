/* udp_usrreq.c 4.1 81/11/07 */

#include "../h/param.h"
#include "../h/mbuf.h"
#include "../inet/inet.h"
#include "../inet/inet_systm.h"
#include "../inet/udp.h"

udp_input(m)
	struct mbuf *m;
{

}

udp_advise(m)
	struct mbuf *m;
{

	m_freem(m);
}

udp_output(xx, m)
	struct mbuf *m;
{

}

udp_usrreq(up, req, m, addr)
	struct socket *up;
	int req;
	struct mbuf *m;
	struct in_addr *addr;
{

	switch (req) {

	case PRU_ATTACH:

	case PRU_DETACH:

	case PRU_CONNECT:

	case PRU_DISCONNECT:

	case PRU_ISCONN:

	case PRU_ISDISCONN:
		break;

	case PRU_RCVD:
		break;

	case PRU_SEND:
		udp_output(0, m);
		break;

	case PRU_ABORT:
	case PRU_CLEAR:
		break;

	case PRU_CONTROL:
		break;

	default:
		panic("udp_usrreq");
}
