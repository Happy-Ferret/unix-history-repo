/*-
 * Copyright (c) 2004-2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef _G_GATE_H_
#define _G_GATE_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>

#include <geom/geom.h>

#define	G_GATE_CLASS_NAME	"GATE"
#define	G_GATE_PROVIDER_NAME	"ggate"
#define	G_GATE_MOD_NAME		"ggate"
#define	G_GATE_CTL_NAME		"ggctl"

#define G_GATE_VERSION		0

/*
 * Maximum number of request that can be stored in
 * the queue when there are no workers.
 */
#define	G_GATE_MAX_QUEUE_SIZE	4096

#define	G_GATE_FLAG_READONLY	0x0001
#define	G_GATE_FLAG_WRITEONLY	0x0002
#define	G_GATE_FLAG_DESTROY	0x1000
#define	G_GATE_USERFLAGS	(G_GATE_FLAG_READONLY | G_GATE_FLAG_WRITEONLY)

#define G_GATE_CMD_CREATE	_IOWR('m', 0, struct g_gate_ctl_create)
#define G_GATE_CMD_DESTROY	_IOWR('m', 1, struct g_gate_ctl_destroy)
#define G_GATE_CMD_START	_IOWR('m', 2, struct g_gate_ctl_io)
#define G_GATE_CMD_DONE		_IOWR('m', 3, struct g_gate_ctl_io)

#define	G_GATE_INFOSIZE		2048

#ifdef _KERNEL
/*
 * 'P:' means 'Protected by'.
 */
struct g_gate_softc {
	int			 sc_unit;		/* P: (read-only) */
	int16_t			 sc_ref;		/* P: g_gate_list_mtx */
	struct g_provider	*sc_provider;		/* P: (read-only) */
	uint32_t		 sc_flags;		/* P: (read-only) */

	struct bio_queue_head	 sc_inqueue;		/* P: sc_queue_mtx */
	struct bio_queue_head	 sc_outqueue;		/* P: sc_queue_mtx */
	struct mtx		 sc_queue_mtx;
	uint32_t		 sc_queue_count;	/* P: sc_queue_mtx */
	uint32_t		 sc_queue_size;		/* P: (read-only) */
	u_int			 sc_timeout;		/* P: (read-only) */
	struct callout		 sc_callout;		/* P: (modified only
							       from callout
							       thread) */
	uintptr_t		 sc_seq;		/* P: (modified only
							       from g_down
							       thread) */
	LIST_ENTRY(g_gate_softc) sc_next;		/* P: g_gate_list_mtx */
	char			 sc_info[G_GATE_INFOSIZE]; /* P: (read-only) */
};
#define	sc_name	sc_provider->geom->name

#define	G_GATE_DEBUG(lvl, ...)	do {					\
	if (g_gate_debug >= (lvl)) {					\
		printf("GEOM_GATE");					\
		if (g_gate_debug > 0)					\
			printf("[%u]", lvl);				\
		printf(": ");						\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
} while (0)
#define	G_GATE_LOGREQ(lvl, bp, ...)	do {				\
	if (g_gate_debug >= (lvl)) {					\
		printf("GEOM_GATE");					\
		if (g_gate_debug > 0)					\
			printf("[%u]", lvl);				\
		printf(": ");						\
		printf(__VA_ARGS__);					\
		printf(" ");						\
		g_print_bio(bp);					\
		printf("\n");						\
	}								\
} while (0)
#endif	/* !_KERNEL */

struct g_gate_ctl_create {
	u_int	gctl_version;
	off_t	gctl_mediasize;
	u_int	gctl_sectorsize;
	u_int	gctl_flags;
	u_int	gctl_maxcount;
	u_int	gctl_timeout;
	char	gctl_info[G_GATE_INFOSIZE];
	int	gctl_unit;	/* out */
};

struct g_gate_ctl_destroy {
	u_int	gctl_version;
	int	gctl_unit;
	int	gctl_force;
};

struct g_gate_ctl_io {
	u_int		 gctl_version;
	int		 gctl_unit;
	uintptr_t	 gctl_seq;
	u_int		 gctl_cmd;
	off_t		 gctl_offset;
	off_t		 gctl_length;
	void		*gctl_data;
	int		 gctl_error;
};
#endif	/* !_G_GATE_H_ */
