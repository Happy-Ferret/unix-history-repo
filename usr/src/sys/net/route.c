/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 *
 *	@(#)route.c	7.17 (Berkeley) %G%
 */
#include "machine/reg.h"
 
#include "param.h"
#include "systm.h"
#include "user.h"
#include "proc.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "domain.h"
#include "protosw.h"
#include "errno.h"
#include "ioctl.h"

#include "if.h"
#include "af.h"
#include "route.h"
#include "raw_cb.h"
#include "../netinet/in.h"
#include "../netinet/in_var.h"

#include "../netns/ns.h"
#include "machine/mtpr.h"
#include "netisr.h"
#define	SA(p) ((struct sockaddr *)(p))

int	rttrash;		/* routes not in table but not freed */
struct	sockaddr wildcard;	/* zero valued cookie for wildcard searches */
int	rthashsize = RTHASHSIZ;	/* for netstat, etc. */

static int rtinits_done = 0;
struct radix_node_head *ns_rnhead, *in_rnhead;
struct radix_node *rn_match(), *rn_delete(), *rn_addroute();
rtinitheads()
{
	if (rtinits_done == 0 &&
	    rn_inithead(&ns_rnhead, 16, AF_NS) &&
	    rn_inithead(&in_rnhead, 32, AF_INET))
		rtinits_done = 1;
}

/*
 * Packet routing routines.
 */
rtalloc(ro)
	register struct route *ro;
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;				 /* XXX */
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1);
}

struct rtentry *
rtalloc1(dst, report)
	register struct sockaddr *dst;
	int  report;
{
	register struct radix_node_head *rnh;
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	int  s = splnet(), err = 0;

	for (rnh = radix_node_head; rnh && (dst->sa_family != rnh->rnh_af); )
		rnh = rnh->rnh_next;
	if (rnh && rnh->rnh_treetop &&
	    (rn = rn_match((caddr_t)dst, rnh->rnh_treetop)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING)) {
			if (err = rtrequest(RTM_RESOLVE, dst, SA(0),
					      SA(0), 0, &newrt))
				goto miss;
		} else
			rt->rt_refcnt++;
	} else {
		rtstat.rts_unreach++;
	miss:	if (report)
			rt_missmsg(RTM_MISS, dst, SA(0), SA(0), SA(0), 0, err);
	}
	splx(s);
	return (newrt);
}

rtfree(rt)
	register struct rtentry *rt;
{
	register struct ifaddr *ifa;
	if (rt == 0)
		panic("rtfree");
	rt->rt_refcnt--;
	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		rttrash--;
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		free((caddr_t)rt, M_RTABLE);
	}
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splnet
 *
 */
rtredirect(dst, gateway, netmask, flags, src, rtp)
	struct sockaddr *dst, *gateway, *netmask, *src;
	int flags;
	struct rtentry **rtp;
{
	register struct rtentry *rt;
	int error = 0;
	short *stat = 0;

	/* verify the gateway is directly reachable */
	if (ifa_ifwithnet(gateway) == 0) {
		error = ENETUNREACH;
		goto done;
	}
	rt = rtalloc1(dst, 0);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (!(flags & RTF_DONE) && rt && !equal(src, rt->rt_gateway))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway))
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if ((rt == 0) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface. 
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
		create:
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			error = rtrequest((int)RTM_ADD, dst, gateway,
				    SA(0), flags,
				    (struct rtentry **)0);
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			if (gateway->sa_len <= rt->rt_gateway->sa_len) {
				Bcopy(gateway, rt->rt_gateway, gateway->sa_len);
				rt->rt_flags |= RTF_MODIFIED;
				flags |= RTF_MODIFIED;
				stat = &rtstat.rts_newgateway;
			} else
				error = ENOSPC;
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
	if (error)
		rtstat.rts_badredirect++;
	else
		(stat && (*stat)++);
	rt_missmsg(RTM_REDIRECT, dst, gateway, netmask, src, flags, error);
}

/*
* Routing table ioctl interface.
*/
rtioctl(req, data)
	int req;
	caddr_t data;
{
#ifndef COMPAT_43
	return (EOPNOTSUPP);
#else
	register struct ortentry *entry = (struct ortentry *)data;
	int error;
	struct sockaddr *netmask = 0;

	if (req == SIOCADDRT)
		req = RTM_ADD;
	else if (req == SIOCDELRT)
		req = RTM_DELETE;
	else
		return (EINVAL);

	if (error = suser(u.u_cred, &u.u_acflag))
		return (error);
#if BYTE_ORDER != BIG_ENDIAN
	if (entry->rt_dst.sa_family == 0 && entry->rt_dst.sa_len < 16) {
		entry->rt_dst.sa_family = entry->rt_dst.sa_len;
		entry->rt_dst.sa_len = 16;
	}
	if (entry->rt_gateway.sa_family == 0 && entry->rt_gateway.sa_len < 16) {
		entry->rt_gateway.sa_family = entry->rt_gateway.sa_len;
		entry->rt_gateway.sa_len = 16;
	}
#else
	if (entry->rt_dst.sa_len == 0)
		entry->rt_dst.sa_len = 16;
	if (entry->rt_gateway.sa_len == 0)
		entry->rt_gateway.sa_len = 16;
#endif
	if ((entry->rt_flags & RTF_HOST) == 0)
		switch (entry->rt_dst.sa_family) {
#ifdef INET
		case AF_INET:
			{
				extern struct sockaddr_in icmpmask;
				struct sockaddr_in *dst_in = 
					(struct sockaddr_in *)&entry->rt_dst;

				in_sockmaskof(dst_in->sin_addr, &icmpmask);
				netmask = (struct sockaddr *)&icmpmask;
			}
			break;
#endif
#ifdef NS
		case AF_NS:
			{
				extern struct sockaddr_ns ns_netmask;
				netmask = (struct sockaddr *)&ns_netmask;
			}
#endif
		}
	error =  rtrequest(req, &(entry->rt_dst), &(entry->rt_gateway), netmask,
				entry->rt_flags, (struct rtentry **)0);
	rt_missmsg((req == RTM_ADD ? RTM_OLDADD : RTM_OLDDEL),
		   &(entry->rt_dst), &(entry->rt_gateway),
		   netmask, SA(0), entry->rt_flags, error);
	return (error);
#endif
}

struct ifaddr *
ifa_ifwithroute(flags, dst, gateway)
int	flags;
struct sockaddr	*dst, *gateway;
{
	struct ifaddr *ifa;
	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = 0;
		if (flags & RTF_HOST) 
			ifa = ifa_ifwithdstaddr(dst);
		if (ifa == 0)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == 0)
		ifa = ifa_ifwithnet(gateway);
	return (ifa);
}

#define ROUNDUP(a) (1 + (((a) - 1) | (sizeof(long) - 1)))

rtrequest(req, dst, gateway, netmask, flags, ret_nrt)
	int req, flags;
	struct sockaddr *dst, *gateway, *netmask;
	struct rtentry **ret_nrt;
{
	int s = splnet(), len, error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct ifaddr *ifa, *ifa_ifwithdstaddr();
	struct sockaddr *ndst;
	u_char af = dst->sa_family;
#define senderr(x) { error = x ; goto bad; }

	if (rtinits_done == 0)
		rtinitheads();
	for (rnh = radix_node_head; rnh && (af != rnh->rnh_af); )
		rnh = rnh->rnh_next;
	if (rnh == 0)
		senderr(ESRCH);
	if (flags & RTF_HOST)
		netmask = 0;
	switch (req) {
	case RTM_DELETE:
		if (ret_nrt && (rt = *ret_nrt)) {
			RTFREE(rt);
			*ret_nrt = 0;
		}
		if ((rn = rn_delete((caddr_t)dst, (caddr_t)netmask, 
					rnh->rnh_treetop)) == 0)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = (struct rtentry *)rn;
		rt->rt_flags &= ~RTF_UP;
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
		rttrash++;
		if (rt->rt_refcnt <= 0)
			rtfree(rt);
		break;

	case RTM_RESOLVE:
		if (ret_nrt == 0 || (rt = *ret_nrt) == 0)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		flags = rt->rt_flags & ~RTF_CLONING;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == 0)
			flags |= RTF_HOST;
		goto makeroute;

	case RTM_ADD:
		if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == 0)
			senderr(ENETUNREACH);
	makeroute:
		len = sizeof (*rt) + ROUNDUP(gateway->sa_len)
		    + ROUNDUP(dst->sa_len);
		R_Malloc(rt, struct rtentry *, len);
		if (rt == 0)
			senderr(ENOBUFS);
		Bzero(rt, len);
		ndst = (struct sockaddr *)(rt + 1);
		if (netmask) {
			rt_maskedcopy(dst, ndst, netmask);
		} else
			Bcopy(dst, ndst, dst->sa_len);
		rn = rn_addroute((caddr_t)ndst, (caddr_t)netmask,
					rnh->rnh_treetop, rt->rt_nodes);
		if (rn == 0) {
			free((caddr_t)rt, M_RTABLE);
			senderr(EEXIST);
		}
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
		rt->rt_flags = RTF_UP | flags;
		rn->rn_key = (caddr_t) ndst; /* == rt_dst */
		rt->rt_gateway = (struct sockaddr *)
					(rn->rn_key + ROUNDUP(dst->sa_len));
		Bcopy(gateway, rt->rt_gateway, gateway->sa_len);
		if (req == RTM_RESOLVE)
			rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : 0));
		if (ret_nrt) {
			*ret_nrt = rt;
			rt->rt_refcnt++;
		}
		break;
	}
bad:
	splx(s);
	return (error);
}

rt_maskedcopy(src, dst, netmask)
struct sockaddr *src, *dst, *netmask;
{
	register u_char *cp1 = (u_char *)src;
	register u_char *cp2 = (u_char *)dst;
	register u_char *cp3 = (u_char *)netmask;
	u_char *cplim = cp2 + *cp3;
	u_char *cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}
/*
 * Set up a routing table entry, normally
 * for an interface.
 */
rtinit(ifa, cmd, flags)
	register struct ifaddr *ifa;
	int cmd, flags;
{
	return rtrequest(cmd, ifa->ifa_dstaddr, ifa->ifa_addr,
		    ifa->ifa_netmask, flags | ifa->ifa_flags, &ifa->ifa_rt);
}
