/*	$NetBSD: svc.h,v 1.17 2000/06/02 22:57:56 fvdl Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 *
 *	from: @(#)svc.h 1.35 88/12/17 SMI
 *	from: @(#)svc.h      1.27    94/04/25 SMI
 * $FreeBSD$
 */

/*
 * svc.h, Server-side remote procedure call interface.
 *
 * Copyright (C) 1986-1993 by Sun Microsystems, Inc.
 */

#ifndef _RPC_SVC_H
#define _RPC_SVC_H
#include <sys/cdefs.h>

/*
 * This interface must manage two items concerning remote procedure calling:
 *
 * 1) An arbitrary number of transport connections upon which rpc requests
 * are received.  The two most notable transports are TCP and UDP;  they are
 * created and registered by routines in svc_tcp.c and svc_udp.c, respectively;
 * they in turn call xprt_register and xprt_unregister.
 *
 * 2) An arbitrary number of locally registered services.  Services are
 * described by the following four data: program number, version number,
 * "service dispatch" function, a transport handle, and a boolean that
 * indicates whether or not the exported program should be registered with a
 * local binder service;  if true the program's number and version and the
 * port number from the transport handle are registered with the binder.
 * These data are registered with the rpc svc system via svc_register.
 *
 * A service's dispatch function is called whenever an rpc request comes in
 * on a transport.  The request's program and version numbers must match
 * those of the registered service.  The dispatch function is passed two
 * parameters, struct svc_req * and SVCXPRT *, defined below.
 */

/*
 *      Service control requests
 */
#define SVCGET_VERSQUIET	1
#define SVCSET_VERSQUIET	2


enum xprt_stat {
	XPRT_DIED,
	XPRT_MOREREQS,
	XPRT_IDLE
};

/*
 * Server side transport handle
 */
typedef struct __rpc_svcxprt {
	int		xp_fd;
	u_short		xp_port;	 /* associated port number */
	const struct xp_ops {
	    /* receive incoming requests */
	    bool_t	(*xp_recv) __P((struct __rpc_svcxprt *,
				struct rpc_msg *));
	    /* get transport status */
	    enum xprt_stat (*xp_stat) __P((struct __rpc_svcxprt *));
	    /* get arguments */
	    bool_t	(*xp_getargs) __P((struct __rpc_svcxprt *, xdrproc_t,
				caddr_t));
	    /* send reply */
	    bool_t	(*xp_reply) __P((struct __rpc_svcxprt *,
				struct rpc_msg *));
	    /* free mem allocated for args */
	    bool_t	(*xp_freeargs) __P((struct __rpc_svcxprt *, xdrproc_t,
				caddr_t));
	    /* destroy this struct */
	    void	(*xp_destroy) __P((struct __rpc_svcxprt *));
	} *xp_ops;
	int		xp_addrlen;	 /* length of remote address */
	struct sockaddr_in xp_raddr;	 /* remote addr. (backward ABI compat) */
	/* XXX - fvdl stick this here for ABI backward compat reasons */
	const struct xp_ops2 {
		/* catch-all function */
		bool_t  (*xp_control) __P((struct __rpc_svcxprt *, const u_int,
					   void *));
	} *xp_ops2;
	char		*xp_tp;		 /* transport provider device name */
	char		*xp_netid;	 /* network token */
	struct netbuf	xp_ltaddr;	 /* local transport address */
	struct netbuf	xp_rtaddr;	 /* remote transport address */
	struct opaque_auth xp_verf;	 /* raw response verifier */
	void		*xp_p1;		 /* private: for use by svc ops */
	void		*xp_p2;		 /* private: for use by svc ops */
	void		*xp_p3;		 /* private: for use by svc lib */
	int		xp_type;	 /* transport type */
} SVCXPRT;

/*
 * Service request
 */
struct svc_req {
	u_int32_t	rq_prog;	/* service program number */
	u_int32_t	rq_vers;	/* service protocol version */
	u_int32_t	rq_proc;	/* the desired procedure */
	struct opaque_auth rq_cred;	/* raw creds from the wire */
	void		*rq_clntcred;	/* read only cooked cred */
	SVCXPRT		*rq_xprt;	/* associated transport */
};

/*
 *  Approved way of getting address of caller
 */
#define svc_getrpccaller(x) (&(x)->xp_rtaddr)

/*
 * FreeBSD-only definition to get the creds of the caller (AF_LOCAL).
 */
#define __svc_getcallercreds(x) ((struct cmsgcred *)(x)->xp_p2)

/*
 * Operations defined on an SVCXPRT handle
 *
 * SVCXPRT		*xprt;
 * struct rpc_msg	*msg;
 * xdrproc_t		 xargs;
 * caddr_t		 argsp;
 */
#define SVC_RECV(xprt, msg)				\
	(*(xprt)->xp_ops->xp_recv)((xprt), (msg))
#define svc_recv(xprt, msg)				\
	(*(xprt)->xp_ops->xp_recv)((xprt), (msg))

#define SVC_STAT(xprt)					\
	(*(xprt)->xp_ops->xp_stat)(xprt)
#define svc_stat(xprt)					\
	(*(xprt)->xp_ops->xp_stat)(xprt)

#define SVC_GETARGS(xprt, xargs, argsp)			\
	(*(xprt)->xp_ops->xp_getargs)((xprt), (xargs), (argsp))
#define svc_getargs(xprt, xargs, argsp)			\
	(*(xprt)->xp_ops->xp_getargs)((xprt), (xargs), (argsp))

#define SVC_REPLY(xprt, msg)				\
	(*(xprt)->xp_ops->xp_reply) ((xprt), (msg))
#define svc_reply(xprt, msg)				\
	(*(xprt)->xp_ops->xp_reply) ((xprt), (msg))

#define SVC_FREEARGS(xprt, xargs, argsp)		\
	(*(xprt)->xp_ops->xp_freeargs)((xprt), (xargs), (argsp))
#define svc_freeargs(xprt, xargs, argsp)		\
	(*(xprt)->xp_ops->xp_freeargs)((xprt), (xargs), (argsp))

#define SVC_DESTROY(xprt)				\
	(*(xprt)->xp_ops->xp_destroy)(xprt)
#define svc_destroy(xprt)				\
	(*(xprt)->xp_ops->xp_destroy)(xprt)

#define SVC_CONTROL(xprt, rq, in)			\
	(*(xprt)->xp_ops2->xp_control)((xprt), (rq), (in))

/*
 * Service registration
 *
 * svc_reg(xprt, prog, vers, dispatch, nconf)
 *	const SVCXPRT *xprt;
 *	const rpcprog_t prog;
 *	const rpcvers_t vers;
 *	const void (*dispatch)();
 *	const struct netconfig *nconf;
 */

__BEGIN_DECLS
extern bool_t	svc_reg __P((SVCXPRT *, const rpcprog_t, const rpcvers_t,
			void (*) __P((struct svc_req *, SVCXPRT *)),
			const struct netconfig *));
__END_DECLS

/*
 * Service un-registration
 *
 * svc_unreg(prog, vers)
 *	const rpcprog_t prog;
 *	const rpcvers_t vers;
 */

__BEGIN_DECLS
extern void	svc_unreg __P((const rpcprog_t, const rpcvers_t));
__END_DECLS

/*
 * Transport registration.
 *
 * xprt_register(xprt)
 *	SVCXPRT *xprt;
 */
__BEGIN_DECLS
extern void	xprt_register	__P((SVCXPRT *));
__END_DECLS

/*
 * Transport un-register
 *
 * xprt_unregister(xprt)
 *	SVCXPRT *xprt;
 */
__BEGIN_DECLS
extern void	xprt_unregister	__P((SVCXPRT *));
__END_DECLS


/*
 * When the service routine is called, it must first check to see if it
 * knows about the procedure;  if not, it should call svcerr_noproc
 * and return.  If so, it should deserialize its arguments via
 * SVC_GETARGS (defined above).  If the deserialization does not work,
 * svcerr_decode should be called followed by a return.  Successful
 * decoding of the arguments should be followed the execution of the
 * procedure's code and a call to svc_sendreply.
 *
 * Also, if the service refuses to execute the procedure due to too-
 * weak authentication parameters, svcerr_weakauth should be called.
 * Note: do not confuse access-control failure with weak authentication!
 *
 * NB: In pure implementations of rpc, the caller always waits for a reply
 * msg.  This message is sent when svc_sendreply is called.
 * Therefore pure service implementations should always call
 * svc_sendreply even if the function logically returns void;  use
 * xdr.h - xdr_void for the xdr routine.  HOWEVER, tcp based rpc allows
 * for the abuse of pure rpc via batched calling or pipelining.  In the
 * case of a batched call, svc_sendreply should NOT be called since
 * this would send a return message, which is what batching tries to avoid.
 * It is the service/protocol writer's responsibility to know which calls are
 * batched and which are not.  Warning: responding to batch calls may
 * deadlock the caller and server processes!
 */

__BEGIN_DECLS
extern bool_t	svc_sendreply	__P((SVCXPRT *, xdrproc_t, char *));
extern void	svcerr_decode	__P((SVCXPRT *));
extern void	svcerr_weakauth	__P((SVCXPRT *));
extern void	svcerr_noproc	__P((SVCXPRT *));
extern void	svcerr_progvers	__P((SVCXPRT *, rpcvers_t, rpcvers_t));
extern void	svcerr_auth	__P((SVCXPRT *, enum auth_stat));
extern void	svcerr_noprog	__P((SVCXPRT *));
extern void	svcerr_systemerr __P((SVCXPRT *));
extern int	rpc_reg __P((rpcprog_t, rpcvers_t, rpcproc_t,
			     char *(*) __P((char *)), xdrproc_t, xdrproc_t,
			     char *));
__END_DECLS

/*
 * Lowest level dispatching -OR- who owns this process anyway.
 * Somebody has to wait for incoming requests and then call the correct
 * service routine.  The routine svc_run does infinite waiting; i.e.,
 * svc_run never returns.
 * Since another (co-existant) package may wish to selectively wait for
 * incoming calls or other events outside of the rpc architecture, the
 * routine svc_getreq is provided.  It must be passed readfds, the
 * "in-place" results of a select system call (see select, section 2).
 */

/*
 * Global keeper of rpc service descriptors in use
 * dynamic; must be inspected before each call to select
 */
extern int svc_maxfd;
#ifdef FD_SETSIZE
extern fd_set svc_fdset;
#define svc_fds svc_fdset.fds_bits[0]	/* compatibility */
#else
extern int svc_fds;
#endif /* def FD_SETSIZE */

/*
 * a small program implemented by the svc_rpc implementation itself;
 * also see clnt.h for protocol numbers.
 */
__BEGIN_DECLS
extern void rpctest_service __P((void));
__END_DECLS

__BEGIN_DECLS
extern void	svc_getreq	__P((int));
extern void	svc_getreqset	__P((fd_set *));
extern void	svc_getreq_common	__P((int));
struct pollfd;
extern void	svc_getreq_poll __P((struct pollfd *, int));

extern void	svc_run		__P((void));
extern void	svc_exit	__P((void));
__END_DECLS

/*
 * Socket to use on svcxxx_create call to get default socket
 */
#define	RPC_ANYSOCK	-1
#define RPC_ANYFD	RPC_ANYSOCK

/*
 * These are the existing service side transport implementations
 */

__BEGIN_DECLS
/*
 * Transport independent svc_create routine.
 */
extern int svc_create __P((void (*) __P((struct svc_req *, SVCXPRT *)),
			   const rpcprog_t, const rpcvers_t, const char *));
/*
 *      void (*dispatch)();             -- dispatch routine
 *      const rpcprog_t prognum;        -- program number
 *      const rpcvers_t versnum;        -- version number
 *      const char *nettype;            -- network type
 */


/*
 * Generic server creation routine. It takes a netconfig structure
 * instead of a nettype.
 */

extern SVCXPRT *svc_tp_create __P((void (*) __P((struct svc_req *, SVCXPRT *)),
				   const rpcprog_t, const rpcvers_t,
				   const struct netconfig *));
        /*
         * void (*dispatch)();            -- dispatch routine
         * const rpcprog_t prognum;       -- program number
         * const rpcvers_t versnum;       -- version number
         * const struct netconfig *nconf; -- netconfig structure
         */


/*
 * Generic TLI create routine
 */
extern SVCXPRT *svc_tli_create __P((const int, const struct netconfig *,
				    const struct t_bind *, const u_int,
				    const u_int));
/*
 *      const int fd;                   -- connection end point
 *      const struct netconfig *nconf;  -- netconfig structure for network
 *      const struct t_bind *bindaddr;  -- local bind address
 *      const u_int sendsz;             -- max sendsize
 *      const u_int recvsz;             -- max recvsize
 */

/*
 * Connectionless and connectionful create routines
 */

extern SVCXPRT *svc_vc_create __P((const int, const u_int, const u_int));
/*
 *      const int fd;                           -- open connection end point
 *      const u_int sendsize;                   -- max send size
 *      const u_int recvsize;                   -- max recv size
 */

/*
 * Added for compatibility to old rpc 4.0. Obsoleted by svc_vc_create().
 */
extern SVCXPRT *svcunix_create __P((int, u_int, u_int, char *));

extern SVCXPRT *svc_dg_create __P((const int, const u_int, const u_int));
        /*
         * const int fd;                                -- open connection
         * const u_int sendsize;                        -- max send size
         * const u_int recvsize;                        -- max recv size
         */


/*
 * the routine takes any *open* connection
 * descriptor as its first input and is used for open connections.
 */
extern SVCXPRT *svc_fd_create __P((const int, const u_int, const u_int));
/*
 *      const int fd;                           -- open connection end point
 *      const u_int sendsize;                   -- max send size
 *      const u_int recvsize;                   -- max recv size
 */

/*
 * Added for compatibility to old rpc 4.0. Obsoleted by svc_fd_create().
 */
extern SVCXPRT *svcunixfd_create __P((int, u_int, u_int));

/*
 * Memory based rpc (for speed check and testing)
 */
extern SVCXPRT *svc_raw_create __P((void));

/*
 * svc_dg_enable_cache() enables the cache on dg transports.
 */
int svc_dg_enablecache __P((SVCXPRT *, const u_int));

int __rpc_get_local_uid(SVCXPRT *_transp, uid_t *_uid);

__END_DECLS


/* for backward compatibility */
#include <rpc/svc_soc.h>

#endif /* !_RPC_SVC_H */
