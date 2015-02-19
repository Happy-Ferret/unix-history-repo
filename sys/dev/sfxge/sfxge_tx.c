/*-
 * Copyright (c) 2010-2011 Solarflare Communications, Inc.
 * All rights reserved.
 *
 * This software was developed in part by Philip Paeps under contract for
 * Solarflare Communications, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Theory of operation:
 *
 * Tx queues allocation and mapping
 *
 * One Tx queue with enabled checksum offload is allocated per Rx channel
 * (event queue).  Also 2 Tx queues (one without checksum offload and one
 * with IP checksum offload only) are allocated and bound to event queue 0.
 * sfxge_txq_type is used as Tx queue label.
 *
 * So, event queue plus label mapping to Tx queue index is:
 *	if event queue index is 0, TxQ-index = TxQ-label * [0..SFXGE_TXQ_NTYPES)
 *	else TxQ-index = SFXGE_TXQ_NTYPES + EvQ-index - 1
 * See sfxge_get_txq_by_label() sfxge_ev.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_vlan_var.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>

#include "common/efx.h"

#include "sfxge.h"
#include "sfxge_tx.h"

/* Set the block level to ensure there is space to generate a
 * large number of descriptors for TSO.  With minimum MSS and
 * maximum mbuf length we might need more than a ring-ful of
 * descriptors, but this should not happen in practice except
 * due to deliberate attack.  In that case we will truncate
 * the output at a packet boundary.  Allow for a reasonable
 * minimum MSS of 512.
 */
#define	SFXGE_TSO_MAX_DESC ((65535 / 512) * 2 + SFXGE_TX_MAPPING_MAX_SEG - 1)
#define	SFXGE_TXQ_BLOCK_LEVEL(_entries)	((_entries) - SFXGE_TSO_MAX_DESC)

#ifdef SFXGE_HAVE_MQ

#define	SFXGE_PARAM_TX_DPL_GET_MAX	SFXGE_PARAM(tx_dpl_get_max)
static int sfxge_tx_dpl_get_max = SFXGE_TX_DPL_GET_PKT_LIMIT_DEFAULT;
TUNABLE_INT(SFXGE_PARAM_TX_DPL_GET_MAX, &sfxge_tx_dpl_get_max);
SYSCTL_INT(_hw_sfxge, OID_AUTO, tx_dpl_get_max, CTLFLAG_RDTUN,
	   &sfxge_tx_dpl_get_max, 0,
	   "Maximum number of any packets in deferred packet get-list");

#define	SFXGE_PARAM_TX_DPL_GET_NON_TCP_MAX \
	SFXGE_PARAM(tx_dpl_get_non_tcp_max)
static int sfxge_tx_dpl_get_non_tcp_max =
	SFXGE_TX_DPL_GET_NON_TCP_PKT_LIMIT_DEFAULT;
TUNABLE_INT(SFXGE_PARAM_TX_DPL_GET_NON_TCP_MAX, &sfxge_tx_dpl_get_non_tcp_max);
SYSCTL_INT(_hw_sfxge, OID_AUTO, tx_dpl_get_non_tcp_max, CTLFLAG_RDTUN,
	   &sfxge_tx_dpl_get_non_tcp_max, 0,
	   "Maximum number of non-TCP packets in deferred packet get-list");

#define	SFXGE_PARAM_TX_DPL_PUT_MAX	SFXGE_PARAM(tx_dpl_put_max)
static int sfxge_tx_dpl_put_max = SFXGE_TX_DPL_PUT_PKT_LIMIT_DEFAULT;
TUNABLE_INT(SFXGE_PARAM_TX_DPL_PUT_MAX, &sfxge_tx_dpl_put_max);
SYSCTL_INT(_hw_sfxge, OID_AUTO, tx_dpl_put_max, CTLFLAG_RDTUN,
	   &sfxge_tx_dpl_put_max, 0,
	   "Maximum number of any packets in deferred packet put-list");

#endif


/* Forward declarations. */
static void sfxge_tx_qdpl_service(struct sfxge_txq *txq);
static void sfxge_tx_qlist_post(struct sfxge_txq *txq);
static void sfxge_tx_qunblock(struct sfxge_txq *txq);
static int sfxge_tx_queue_tso(struct sfxge_txq *txq, struct mbuf *mbuf,
			      const bus_dma_segment_t *dma_seg, int n_dma_seg);

void
sfxge_tx_qcomplete(struct sfxge_txq *txq, struct sfxge_evq *evq)
{
	unsigned int completed;

	SFXGE_EVQ_LOCK_ASSERT_OWNED(evq);

	completed = txq->completed;
	while (completed != txq->pending) {
		struct sfxge_tx_mapping *stmp;
		unsigned int id;

		id = completed++ & txq->ptr_mask;

		stmp = &txq->stmp[id];
		if (stmp->flags & TX_BUF_UNMAP) {
			bus_dmamap_unload(txq->packet_dma_tag, stmp->map);
			if (stmp->flags & TX_BUF_MBUF) {
				struct mbuf *m = stmp->u.mbuf;
				do
					m = m_free(m);
				while (m != NULL);
			} else {
				free(stmp->u.heap_buf, M_SFXGE);
			}
			stmp->flags = 0;
		}
	}
	txq->completed = completed;

	/* Check whether we need to unblock the queue. */
	mb();
	if (txq->blocked) {
		unsigned int level;

		level = txq->added - txq->completed;
		if (level <= SFXGE_TXQ_UNBLOCK_LEVEL(txq->entries))
			sfxge_tx_qunblock(txq);
	}
}

#ifdef SFXGE_HAVE_MQ

static unsigned int
sfxge_is_mbuf_non_tcp(struct mbuf *mbuf)
{
	/* Absense of TCP checksum flags does not mean that it is non-TCP
	 * but it should be true if user wants to achieve high throughput.
	 */
	return (!(mbuf->m_pkthdr.csum_flags & (CSUM_IP_TCP | CSUM_IP6_TCP)));
}

/*
 * Reorder the put list and append it to the get list.
 */
static void
sfxge_tx_qdpl_swizzle(struct sfxge_txq *txq)
{
	struct sfxge_tx_dpl *stdp;
	struct mbuf *mbuf, *get_next, **get_tailp;
	volatile uintptr_t *putp;
	uintptr_t put;
	unsigned int count;
	unsigned int non_tcp_count;

	SFXGE_TXQ_LOCK_ASSERT_OWNED(txq);

	stdp = &txq->dpl;

	/* Acquire the put list. */
	putp = &stdp->std_put;
	put = atomic_readandclear_ptr(putp);
	mbuf = (void *)put;

	if (mbuf == NULL)
		return;

	/* Reverse the put list. */
	get_tailp = &mbuf->m_nextpkt;
	get_next = NULL;

	count = 0;
	non_tcp_count = 0;
	do {
		struct mbuf *put_next;

		non_tcp_count += sfxge_is_mbuf_non_tcp(mbuf);
		put_next = mbuf->m_nextpkt;
		mbuf->m_nextpkt = get_next;
		get_next = mbuf;
		mbuf = put_next;

		count++;
	} while (mbuf != NULL);

	/* Append the reversed put list to the get list. */
	KASSERT(*get_tailp == NULL, ("*get_tailp != NULL"));
	*stdp->std_getp = get_next;
	stdp->std_getp = get_tailp;
	stdp->std_get_count += count;
	stdp->std_get_non_tcp_count += non_tcp_count;
}

#endif /* SFXGE_HAVE_MQ */

static void
sfxge_tx_qreap(struct sfxge_txq *txq)
{
	SFXGE_TXQ_LOCK_ASSERT_OWNED(txq);

	txq->reaped = txq->completed;
}

static void
sfxge_tx_qlist_post(struct sfxge_txq *txq)
{
	unsigned int old_added;
	unsigned int level;
	int rc;

	SFXGE_TXQ_LOCK_ASSERT_OWNED(txq);

	KASSERT(txq->n_pend_desc != 0, ("txq->n_pend_desc == 0"));
	KASSERT(txq->n_pend_desc <= SFXGE_TSO_MAX_DESC,
		("txq->n_pend_desc too large"));
	KASSERT(!txq->blocked, ("txq->blocked"));

	old_added = txq->added;

	/* Post the fragment list. */
	rc = efx_tx_qpost(txq->common, txq->pend_desc, txq->n_pend_desc,
			  txq->reaped, &txq->added);
	KASSERT(rc == 0, ("efx_tx_qpost() failed"));

	/* If efx_tx_qpost() had to refragment, our information about
	 * buffers to free may be associated with the wrong
	 * descriptors.
	 */
	KASSERT(txq->added - old_added == txq->n_pend_desc,
		("efx_tx_qpost() refragmented descriptors"));

	level = txq->added - txq->reaped;
	KASSERT(level <= txq->entries, ("overfilled TX queue"));

	/* Clear the fragment list. */
	txq->n_pend_desc = 0;

	/* Have we reached the block level? */
	if (level < SFXGE_TXQ_BLOCK_LEVEL(txq->entries))
		return;

	/* Reap, and check again */
	sfxge_tx_qreap(txq);
	level = txq->added - txq->reaped;
	if (level < SFXGE_TXQ_BLOCK_LEVEL(txq->entries))
		return;

	txq->blocked = 1;

	/*
	 * Avoid a race with completion interrupt handling that could leave
	 * the queue blocked.
	 */
	mb();
	sfxge_tx_qreap(txq);
	level = txq->added - txq->reaped;
	if (level < SFXGE_TXQ_BLOCK_LEVEL(txq->entries)) {
		mb();
		txq->blocked = 0;
	}
}

static int sfxge_tx_queue_mbuf(struct sfxge_txq *txq, struct mbuf *mbuf)
{
	bus_dmamap_t *used_map;
	bus_dmamap_t map;
	bus_dma_segment_t dma_seg[SFXGE_TX_MAPPING_MAX_SEG];
	unsigned int id;
	struct sfxge_tx_mapping *stmp;
	efx_buffer_t *desc;
	int n_dma_seg;
	int rc;
	int i;

	KASSERT(!txq->blocked, ("txq->blocked"));

	if (mbuf->m_pkthdr.csum_flags & CSUM_TSO)
		prefetch_read_many(mbuf->m_data);

	if (txq->init_state != SFXGE_TXQ_STARTED) {
		rc = EINTR;
		goto reject;
	}

	/* Load the packet for DMA. */
	id = txq->added & txq->ptr_mask;
	stmp = &txq->stmp[id];
	rc = bus_dmamap_load_mbuf_sg(txq->packet_dma_tag, stmp->map,
				     mbuf, dma_seg, &n_dma_seg, 0);
	if (rc == EFBIG) {
		/* Try again. */
		struct mbuf *new_mbuf = m_collapse(mbuf, M_NOWAIT,
						   SFXGE_TX_MAPPING_MAX_SEG);
		if (new_mbuf == NULL)
			goto reject;
		++txq->collapses;
		mbuf = new_mbuf;
		rc = bus_dmamap_load_mbuf_sg(txq->packet_dma_tag,
					     stmp->map, mbuf,
					     dma_seg, &n_dma_seg, 0);
	}
	if (rc != 0)
		goto reject;

	/* Make the packet visible to the hardware. */
	bus_dmamap_sync(txq->packet_dma_tag, stmp->map, BUS_DMASYNC_PREWRITE);

	used_map = &stmp->map;

	if (mbuf->m_pkthdr.csum_flags & CSUM_TSO) {
		rc = sfxge_tx_queue_tso(txq, mbuf, dma_seg, n_dma_seg);
		if (rc < 0)
			goto reject_mapped;
		stmp = &txq->stmp[rc];
	} else {
		/* Add the mapping to the fragment list, and set flags
		 * for the buffer.
		 */
		i = 0;
		for (;;) {
			desc = &txq->pend_desc[i];
			desc->eb_addr = dma_seg[i].ds_addr;
			desc->eb_size = dma_seg[i].ds_len;
			if (i == n_dma_seg - 1) {
				desc->eb_eop = 1;
				break;
			}
			desc->eb_eop = 0;
			i++;

			stmp->flags = 0;
			if (__predict_false(stmp ==
					    &txq->stmp[txq->ptr_mask]))
				stmp = &txq->stmp[0];
			else
				stmp++;
		}
		txq->n_pend_desc = n_dma_seg;
	}

	/*
	 * If the mapping required more than one descriptor
	 * then we need to associate the DMA map with the last
	 * descriptor, not the first.
	 */
	if (used_map != &stmp->map) {
		map = stmp->map;
		stmp->map = *used_map;
		*used_map = map;
	}

	stmp->u.mbuf = mbuf;
	stmp->flags = TX_BUF_UNMAP | TX_BUF_MBUF;

	/* Post the fragment list. */
	sfxge_tx_qlist_post(txq);

	return (0);

reject_mapped:
	bus_dmamap_unload(txq->packet_dma_tag, *used_map);
reject:
	/* Drop the packet on the floor. */
	m_freem(mbuf);
	++txq->drops;

	return (rc);
}

#ifdef SFXGE_HAVE_MQ

/*
 * Drain the deferred packet list into the transmit queue.
 */
static void
sfxge_tx_qdpl_drain(struct sfxge_txq *txq)
{
	struct sfxge_softc *sc;
	struct sfxge_tx_dpl *stdp;
	struct mbuf *mbuf, *next;
	unsigned int count;
	unsigned int non_tcp_count;
	unsigned int pushed;
	int rc;

	SFXGE_TXQ_LOCK_ASSERT_OWNED(txq);

	sc = txq->sc;
	stdp = &txq->dpl;
	pushed = txq->added;

	prefetch_read_many(sc->enp);
	prefetch_read_many(txq->common);

	mbuf = stdp->std_get;
	count = stdp->std_get_count;
	non_tcp_count = stdp->std_get_non_tcp_count;

	if (count > stdp->std_get_hiwat)
		stdp->std_get_hiwat = count;

	while (count != 0) {
		KASSERT(mbuf != NULL, ("mbuf == NULL"));

		next = mbuf->m_nextpkt;
		mbuf->m_nextpkt = NULL;

		ETHER_BPF_MTAP(sc->ifnet, mbuf); /* packet capture */

		if (next != NULL)
			prefetch_read_many(next);

		rc = sfxge_tx_queue_mbuf(txq, mbuf);
		--count;
		non_tcp_count -= sfxge_is_mbuf_non_tcp(mbuf);
		mbuf = next;
		if (rc != 0)
			continue;

		if (txq->blocked)
			break;

		/* Push the fragments to the hardware in batches. */
		if (txq->added - pushed >= SFXGE_TX_BATCH) {
			efx_tx_qpush(txq->common, txq->added);
			pushed = txq->added;
		}
	}

	if (count == 0) {
		KASSERT(mbuf == NULL, ("mbuf != NULL"));
		KASSERT(non_tcp_count == 0,
			("inconsistent TCP/non-TCP detection"));
		stdp->std_get = NULL;
		stdp->std_get_count = 0;
		stdp->std_get_non_tcp_count = 0;
		stdp->std_getp = &stdp->std_get;
	} else {
		stdp->std_get = mbuf;
		stdp->std_get_count = count;
		stdp->std_get_non_tcp_count = non_tcp_count;
	}

	if (txq->added != pushed)
		efx_tx_qpush(txq->common, txq->added);

	KASSERT(txq->blocked || stdp->std_get_count == 0,
		("queue unblocked but count is non-zero"));
}

#define	SFXGE_TX_QDPL_PENDING(_txq)					\
	((_txq)->dpl.std_put != 0)

/*
 * Service the deferred packet list.
 *
 * NOTE: drops the txq mutex!
 */
static void
sfxge_tx_qdpl_service(struct sfxge_txq *txq)
{
	SFXGE_TXQ_LOCK_ASSERT_OWNED(txq);

	do {
		if (SFXGE_TX_QDPL_PENDING(txq))
			sfxge_tx_qdpl_swizzle(txq);

		if (!txq->blocked)
			sfxge_tx_qdpl_drain(txq);

		SFXGE_TXQ_UNLOCK(txq);
	} while (SFXGE_TX_QDPL_PENDING(txq) &&
		 SFXGE_TXQ_TRYLOCK(txq));
}

/*
 * Put a packet on the deferred packet list.
 *
 * If we are called with the txq lock held, we put the packet on the "get
 * list", otherwise we atomically push it on the "put list".  The swizzle
 * function takes care of ordering.
 *
 * The length of the put list is bounded by SFXGE_TX_MAX_DEFFERED.  We
 * overload the csum_data field in the mbuf to keep track of this length
 * because there is no cheap alternative to avoid races.
 */
static int
sfxge_tx_qdpl_put(struct sfxge_txq *txq, struct mbuf *mbuf, int locked)
{
	struct sfxge_tx_dpl *stdp;

	stdp = &txq->dpl;

	KASSERT(mbuf->m_nextpkt == NULL, ("mbuf->m_nextpkt != NULL"));

	if (locked) {
		SFXGE_TXQ_LOCK_ASSERT_OWNED(txq);

		sfxge_tx_qdpl_swizzle(txq);

		if (stdp->std_get_count >= stdp->std_get_max) {
			txq->get_overflow++;
			return (ENOBUFS);
		}
		if (sfxge_is_mbuf_non_tcp(mbuf)) {
			if (stdp->std_get_non_tcp_count >=
			    stdp->std_get_non_tcp_max) {
				txq->get_non_tcp_overflow++;
				return (ENOBUFS);
			}
			stdp->std_get_non_tcp_count++;
		}

		*(stdp->std_getp) = mbuf;
		stdp->std_getp = &mbuf->m_nextpkt;
		stdp->std_get_count++;
	} else {
		volatile uintptr_t *putp;
		uintptr_t old;
		uintptr_t new;
		unsigned old_len;

		putp = &stdp->std_put;
		new = (uintptr_t)mbuf;

		do {
			old = *putp;
			if (old != 0) {
				struct mbuf *mp = (struct mbuf *)old;
				old_len = mp->m_pkthdr.csum_data;
			} else
				old_len = 0;
			if (old_len >= stdp->std_put_max) {
				atomic_add_long(&txq->put_overflow, 1);
				return (ENOBUFS);
			}
			mbuf->m_pkthdr.csum_data = old_len + 1;
			mbuf->m_nextpkt = (void *)old;
		} while (atomic_cmpset_ptr(putp, old, new) == 0);
	}

	return (0);
}

/*
 * Called from if_transmit - will try to grab the txq lock and enqueue to the
 * put list if it succeeds, otherwise will push onto the defer list.
 */
int
sfxge_tx_packet_add(struct sfxge_txq *txq, struct mbuf *m)
{
	int locked;
	int rc;

	if (!SFXGE_LINK_UP(txq->sc)) {
		rc = ENETDOWN;
		atomic_add_long(&txq->netdown_drops, 1);
		goto fail;
	}

	/*
	 * Try to grab the txq lock.  If we are able to get the lock,
	 * the packet will be appended to the "get list" of the deferred
	 * packet list.  Otherwise, it will be pushed on the "put list".
	 */
	locked = SFXGE_TXQ_TRYLOCK(txq);

	if (sfxge_tx_qdpl_put(txq, m, locked) != 0) {
		if (locked)
			SFXGE_TXQ_UNLOCK(txq);
		rc = ENOBUFS;
		goto fail;
	}

	/*
	 * Try to grab the lock again.
	 *
	 * If we are able to get the lock, we need to process the deferred
	 * packet list.  If we are not able to get the lock, another thread
	 * is processing the list.
	 */
	if (!locked)
		locked = SFXGE_TXQ_TRYLOCK(txq);

	if (locked) {
		/* Try to service the list. */
		sfxge_tx_qdpl_service(txq);
		/* Lock has been dropped. */
	}

	return (0);

fail:
	m_freem(m);
	return (rc);
}

static void
sfxge_tx_qdpl_flush(struct sfxge_txq *txq)
{
	struct sfxge_tx_dpl *stdp = &txq->dpl;
	struct mbuf *mbuf, *next;

	SFXGE_TXQ_LOCK(txq);

	sfxge_tx_qdpl_swizzle(txq);
	for (mbuf = stdp->std_get; mbuf != NULL; mbuf = next) {
		next = mbuf->m_nextpkt;
		m_freem(mbuf);
	}
	stdp->std_get = NULL;
	stdp->std_get_count = 0;
	stdp->std_get_non_tcp_count = 0;
	stdp->std_getp = &stdp->std_get;

	SFXGE_TXQ_UNLOCK(txq);
}

void
sfxge_if_qflush(struct ifnet *ifp)
{
	struct sfxge_softc *sc;
	int i;

	sc = ifp->if_softc;

	for (i = 0; i < sc->txq_count; i++)
		sfxge_tx_qdpl_flush(sc->txq[i]);
}

/*
 * TX start -- called by the stack.
 */
int
sfxge_if_transmit(struct ifnet *ifp, struct mbuf *m)
{
	struct sfxge_softc *sc;
	struct sfxge_txq *txq;
	int rc;

	sc = (struct sfxge_softc *)ifp->if_softc;

	KASSERT(ifp->if_flags & IFF_UP, ("interface not up"));

	/* Pick the desired transmit queue. */
	if (m->m_pkthdr.csum_flags & (CSUM_DELAY_DATA | CSUM_TSO)) {
		int index = 0;

		/* check if flowid is set */
		if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
			uint32_t hash = m->m_pkthdr.flowid;

			index = sc->rx_indir_table[hash % SFXGE_RX_SCALE_MAX];
		}
		txq = sc->txq[SFXGE_TXQ_IP_TCP_UDP_CKSUM + index];
	} else if (m->m_pkthdr.csum_flags & CSUM_DELAY_IP) {
		txq = sc->txq[SFXGE_TXQ_IP_CKSUM];
	} else {
		txq = sc->txq[SFXGE_TXQ_NON_CKSUM];
	}

	rc = sfxge_tx_packet_add(txq, m);

	return (rc);
}

#else /* !SFXGE_HAVE_MQ */

static void sfxge_if_start_locked(struct ifnet *ifp)
{
	struct sfxge_softc *sc = ifp->if_softc;
	struct sfxge_txq *txq;
	struct mbuf *mbuf;
	unsigned int pushed[SFXGE_TXQ_NTYPES];
	unsigned int q_index;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	if (!sc->port.link_up)
		return;

	for (q_index = 0; q_index < SFXGE_TXQ_NTYPES; q_index++) {
		txq = sc->txq[q_index];
		pushed[q_index] = txq->added;
	}

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, mbuf);
		if (mbuf == NULL)
			break;

		ETHER_BPF_MTAP(ifp, mbuf); /* packet capture */

		/* Pick the desired transmit queue. */
		if (mbuf->m_pkthdr.csum_flags & (CSUM_DELAY_DATA | CSUM_TSO))
			q_index = SFXGE_TXQ_IP_TCP_UDP_CKSUM;
		else if (mbuf->m_pkthdr.csum_flags & CSUM_DELAY_IP)
			q_index = SFXGE_TXQ_IP_CKSUM;
		else
			q_index = SFXGE_TXQ_NON_CKSUM;
		txq = sc->txq[q_index];

		if (sfxge_tx_queue_mbuf(txq, mbuf) != 0)
			continue;

		if (txq->blocked) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		/* Push the fragments to the hardware in batches. */
		if (txq->added - pushed[q_index] >= SFXGE_TX_BATCH) {
			efx_tx_qpush(txq->common, txq->added);
			pushed[q_index] = txq->added;
		}
	}

	for (q_index = 0; q_index < SFXGE_TXQ_NTYPES; q_index++) {
		txq = sc->txq[q_index];
		if (txq->added != pushed[q_index])
			efx_tx_qpush(txq->common, txq->added);
	}
}

void sfxge_if_start(struct ifnet *ifp)
{
	struct sfxge_softc *sc = ifp->if_softc;

	SFXGE_TXQ_LOCK(sc->txq[0]);
	sfxge_if_start_locked(ifp);
	SFXGE_TXQ_UNLOCK(sc->txq[0]);
}

static void
sfxge_tx_qdpl_service(struct sfxge_txq *txq)
{
	struct ifnet *ifp = txq->sc->ifnet;

	SFXGE_TXQ_LOCK_ASSERT_OWNED(txq);
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sfxge_if_start_locked(ifp);
	SFXGE_TXQ_UNLOCK(txq);
}

#endif /* SFXGE_HAVE_MQ */

/*
 * Software "TSO".  Not quite as good as doing it in hardware, but
 * still faster than segmenting in the stack.
 */

struct sfxge_tso_state {
	/* Output position */
	unsigned out_len;	/* Remaining length in current segment */
	unsigned seqnum;	/* Current sequence number */
	unsigned packet_space;	/* Remaining space in current packet */

	/* Input position */
	uint64_t dma_addr;	/* DMA address of current position */
	unsigned in_len;	/* Remaining length in current mbuf */

	const struct mbuf *mbuf; /* Input mbuf (head of chain) */
	u_short protocol;	/* Network protocol (after VLAN decap) */
	ssize_t nh_off;		/* Offset of network header */
	ssize_t tcph_off;	/* Offset of TCP header */
	unsigned header_len;	/* Number of bytes of header */
	unsigned seg_size;	/* TCP segment size */
};

static const struct ip *tso_iph(const struct sfxge_tso_state *tso)
{
	KASSERT(tso->protocol == htons(ETHERTYPE_IP),
		("tso_iph() in non-IPv4 state"));
	return (const struct ip *)(tso->mbuf->m_data + tso->nh_off);
}
static __unused const struct ip6_hdr *tso_ip6h(const struct sfxge_tso_state *tso)
{
	KASSERT(tso->protocol == htons(ETHERTYPE_IPV6),
		("tso_ip6h() in non-IPv6 state"));
	return (const struct ip6_hdr *)(tso->mbuf->m_data + tso->nh_off);
}
static const struct tcphdr *tso_tcph(const struct sfxge_tso_state *tso)
{
	return (const struct tcphdr *)(tso->mbuf->m_data + tso->tcph_off);
}

/* Size of preallocated TSO header buffers.  Larger blocks must be
 * allocated from the heap.
 */
#define	TSOH_STD_SIZE	128

/* At most half the descriptors in the queue at any time will refer to
 * a TSO header buffer, since they must always be followed by a
 * payload descriptor referring to an mbuf.
 */
#define	TSOH_COUNT(_txq_entries)	((_txq_entries) / 2u)
#define	TSOH_PER_PAGE	(PAGE_SIZE / TSOH_STD_SIZE)
#define	TSOH_PAGE_COUNT(_txq_entries)	\
	((TSOH_COUNT(_txq_entries) + TSOH_PER_PAGE - 1) / TSOH_PER_PAGE)

static int tso_init(struct sfxge_txq *txq)
{
	struct sfxge_softc *sc = txq->sc;
	unsigned int tsoh_page_count = TSOH_PAGE_COUNT(sc->txq_entries);
	int i, rc;

	/* Allocate TSO header buffers */
	txq->tsoh_buffer = malloc(tsoh_page_count * sizeof(txq->tsoh_buffer[0]),
				  M_SFXGE, M_WAITOK);

	for (i = 0; i < tsoh_page_count; i++) {
		rc = sfxge_dma_alloc(sc, PAGE_SIZE, &txq->tsoh_buffer[i]);
		if (rc != 0)
			goto fail;
	}

	return (0);

fail:
	while (i-- > 0)
		sfxge_dma_free(&txq->tsoh_buffer[i]);
	free(txq->tsoh_buffer, M_SFXGE);
	txq->tsoh_buffer = NULL;
	return (rc);
}

static void tso_fini(struct sfxge_txq *txq)
{
	int i;

	if (txq->tsoh_buffer != NULL) {
		for (i = 0; i < TSOH_PAGE_COUNT(txq->sc->txq_entries); i++)
			sfxge_dma_free(&txq->tsoh_buffer[i]);
		free(txq->tsoh_buffer, M_SFXGE);
	}
}

static void tso_start(struct sfxge_tso_state *tso, struct mbuf *mbuf)
{
	struct ether_header *eh = mtod(mbuf, struct ether_header *);

	tso->mbuf = mbuf;

	/* Find network protocol and header */
	tso->protocol = eh->ether_type;
	if (tso->protocol == htons(ETHERTYPE_VLAN)) {
		struct ether_vlan_header *veh =
			mtod(mbuf, struct ether_vlan_header *);
		tso->protocol = veh->evl_proto;
		tso->nh_off = sizeof(*veh);
	} else {
		tso->nh_off = sizeof(*eh);
	}

	/* Find TCP header */
	if (tso->protocol == htons(ETHERTYPE_IP)) {
		KASSERT(tso_iph(tso)->ip_p == IPPROTO_TCP,
			("TSO required on non-TCP packet"));
		tso->tcph_off = tso->nh_off + 4 * tso_iph(tso)->ip_hl;
	} else {
		KASSERT(tso->protocol == htons(ETHERTYPE_IPV6),
			("TSO required on non-IP packet"));
		KASSERT(tso_ip6h(tso)->ip6_nxt == IPPROTO_TCP,
			("TSO required on non-TCP packet"));
		tso->tcph_off = tso->nh_off + sizeof(struct ip6_hdr);
	}

	tso->header_len = tso->tcph_off + 4 * tso_tcph(tso)->th_off;
	tso->seg_size = mbuf->m_pkthdr.tso_segsz;

	tso->seqnum = ntohl(tso_tcph(tso)->th_seq);

	/* These flags must not be duplicated */
	KASSERT(!(tso_tcph(tso)->th_flags & (TH_URG | TH_SYN | TH_RST)),
		("incompatible TCP flag on TSO packet"));

	tso->out_len = mbuf->m_pkthdr.len - tso->header_len;
}

/*
 * tso_fill_packet_with_fragment - form descriptors for the current fragment
 *
 * Form descriptors for the current fragment, until we reach the end
 * of fragment or end-of-packet.  Return 0 on success, 1 if not enough
 * space.
 */
static void tso_fill_packet_with_fragment(struct sfxge_txq *txq,
					  struct sfxge_tso_state *tso)
{
	efx_buffer_t *desc;
	int n;

	if (tso->in_len == 0 || tso->packet_space == 0)
		return;

	KASSERT(tso->in_len > 0, ("TSO input length went negative"));
	KASSERT(tso->packet_space > 0, ("TSO packet space went negative"));

	n = min(tso->in_len, tso->packet_space);

	tso->packet_space -= n;
	tso->out_len -= n;
	tso->in_len -= n;

	desc = &txq->pend_desc[txq->n_pend_desc++];
	desc->eb_addr = tso->dma_addr;
	desc->eb_size = n;
	desc->eb_eop = tso->out_len == 0 || tso->packet_space == 0;

	tso->dma_addr += n;
}

/* Callback from bus_dmamap_load() for long TSO headers. */
static void tso_map_long_header(void *dma_addr_ret,
				bus_dma_segment_t *segs, int nseg,
				int error)
{
	*(uint64_t *)dma_addr_ret = ((__predict_true(error == 0) &&
				      __predict_true(nseg == 1)) ?
				     segs->ds_addr : 0);
}

/*
 * tso_start_new_packet - generate a new header and prepare for the new packet
 *
 * Generate a new header and prepare for the new packet.  Return 0 on
 * success, or an error code if failed to alloc header.
 */
static int tso_start_new_packet(struct sfxge_txq *txq,
				struct sfxge_tso_state *tso,
				unsigned int id)
{
	struct sfxge_tx_mapping *stmp = &txq->stmp[id];
	struct tcphdr *tsoh_th;
	unsigned ip_length;
	caddr_t header;
	uint64_t dma_addr;
	bus_dmamap_t map;
	efx_buffer_t *desc;
	int rc;

	/* Allocate a DMA-mapped header buffer. */
	if (__predict_true(tso->header_len <= TSOH_STD_SIZE)) {
		unsigned int page_index = (id / 2) / TSOH_PER_PAGE;
		unsigned int buf_index = (id / 2) % TSOH_PER_PAGE;

		header = (txq->tsoh_buffer[page_index].esm_base +
			  buf_index * TSOH_STD_SIZE);
		dma_addr = (txq->tsoh_buffer[page_index].esm_addr +
			    buf_index * TSOH_STD_SIZE);
		map = txq->tsoh_buffer[page_index].esm_map;

		stmp->flags = 0;
	} else {
		/* We cannot use bus_dmamem_alloc() as that may sleep */
		header = malloc(tso->header_len, M_SFXGE, M_NOWAIT);
		if (__predict_false(!header))
			return (ENOMEM);
		rc = bus_dmamap_load(txq->packet_dma_tag, stmp->map,
				     header, tso->header_len,
				     tso_map_long_header, &dma_addr,
				     BUS_DMA_NOWAIT);
		if (__predict_false(dma_addr == 0)) {
			if (rc == 0) {
				/* Succeeded but got >1 segment */
				bus_dmamap_unload(txq->packet_dma_tag,
						  stmp->map);
				rc = EINVAL;
			}
			free(header, M_SFXGE);
			return (rc);
		}
		map = stmp->map;

		txq->tso_long_headers++;
		stmp->u.heap_buf = header;
		stmp->flags = TX_BUF_UNMAP;
	}

	tsoh_th = (struct tcphdr *)(header + tso->tcph_off);

	/* Copy and update the headers. */
	m_copydata(tso->mbuf, 0, tso->header_len, header);

	tsoh_th->th_seq = htonl(tso->seqnum);
	tso->seqnum += tso->seg_size;
	if (tso->out_len > tso->seg_size) {
		/* This packet will not finish the TSO burst. */
		ip_length = tso->header_len - tso->nh_off + tso->seg_size;
		tsoh_th->th_flags &= ~(TH_FIN | TH_PUSH);
	} else {
		/* This packet will be the last in the TSO burst. */
		ip_length = tso->header_len - tso->nh_off + tso->out_len;
	}

	if (tso->protocol == htons(ETHERTYPE_IP)) {
		struct ip *tsoh_iph = (struct ip *)(header + tso->nh_off);
		tsoh_iph->ip_len = htons(ip_length);
		/* XXX We should increment ip_id, but FreeBSD doesn't
		 * currently allocate extra IDs for multiple segments.
		 */
	} else {
		struct ip6_hdr *tsoh_iph =
			(struct ip6_hdr *)(header + tso->nh_off);
		tsoh_iph->ip6_plen = htons(ip_length - sizeof(*tsoh_iph));
	}

	/* Make the header visible to the hardware. */
	bus_dmamap_sync(txq->packet_dma_tag, map, BUS_DMASYNC_PREWRITE);

	tso->packet_space = tso->seg_size;
	txq->tso_packets++;

	/* Form a descriptor for this header. */
	desc = &txq->pend_desc[txq->n_pend_desc++];
	desc->eb_addr = dma_addr;
	desc->eb_size = tso->header_len;
	desc->eb_eop = 0;

	return (0);
}

static int
sfxge_tx_queue_tso(struct sfxge_txq *txq, struct mbuf *mbuf,
		   const bus_dma_segment_t *dma_seg, int n_dma_seg)
{
	struct sfxge_tso_state tso;
	unsigned int id, next_id;
	unsigned skipped = 0;

	tso_start(&tso, mbuf);

	while (dma_seg->ds_len + skipped <= tso.header_len) {
		skipped += dma_seg->ds_len;
		--n_dma_seg;
		KASSERT(n_dma_seg, ("no payload found in TSO packet"));
		++dma_seg;
	}
	tso.in_len = dma_seg->ds_len + (tso.header_len - skipped);
	tso.dma_addr = dma_seg->ds_addr + (tso.header_len - skipped);

	id = txq->added & txq->ptr_mask;
	if (__predict_false(tso_start_new_packet(txq, &tso, id)))
		return (-1);

	while (1) {
		id = (id + 1) & txq->ptr_mask;
		tso_fill_packet_with_fragment(txq, &tso);

		/* Move onto the next fragment? */
		if (tso.in_len == 0) {
			--n_dma_seg;
			if (n_dma_seg == 0)
				break;
			++dma_seg;
			tso.in_len = dma_seg->ds_len;
			tso.dma_addr = dma_seg->ds_addr;
		}

		/* End of packet? */
		if (tso.packet_space == 0) {
			/* If the queue is now full due to tiny MSS,
			 * or we can't create another header, discard
			 * the remainder of the input mbuf but do not
			 * roll back the work we have done.
			 */
			if (txq->n_pend_desc >
			    SFXGE_TSO_MAX_DESC - (1 + SFXGE_TX_MAPPING_MAX_SEG)) {
				txq->tso_pdrop_too_many++;
				break;
			}
			next_id = (id + 1) & txq->ptr_mask;
			if (__predict_false(tso_start_new_packet(txq, &tso,
								 next_id))) {
				txq->tso_pdrop_no_rsrc++;
				break;
			}
			id = next_id;
		}
	}

	txq->tso_bursts++;
	return (id);
}

static void
sfxge_tx_qunblock(struct sfxge_txq *txq)
{
	struct sfxge_softc *sc;
	struct sfxge_evq *evq;

	sc = txq->sc;
	evq = sc->evq[txq->evq_index];

	SFXGE_EVQ_LOCK_ASSERT_OWNED(evq);

	if (txq->init_state != SFXGE_TXQ_STARTED)
		return;

	SFXGE_TXQ_LOCK(txq);

	if (txq->blocked) {
		unsigned int level;

		level = txq->added - txq->completed;
		if (level <= SFXGE_TXQ_UNBLOCK_LEVEL(txq->entries))
			txq->blocked = 0;
	}

	sfxge_tx_qdpl_service(txq);
	/* note: lock has been dropped */
}

void
sfxge_tx_qflush_done(struct sfxge_txq *txq)
{

	txq->flush_state = SFXGE_FLUSH_DONE;
}

static void
sfxge_tx_qstop(struct sfxge_softc *sc, unsigned int index)
{
	struct sfxge_txq *txq;
	struct sfxge_evq *evq;
	unsigned int count;

	txq = sc->txq[index];
	evq = sc->evq[txq->evq_index];

	SFXGE_TXQ_LOCK(txq);

	KASSERT(txq->init_state == SFXGE_TXQ_STARTED,
	    ("txq->init_state != SFXGE_TXQ_STARTED"));

	txq->init_state = SFXGE_TXQ_INITIALIZED;
	txq->flush_state = SFXGE_FLUSH_PENDING;

	/* Flush the transmit queue. */
	efx_tx_qflush(txq->common);

	SFXGE_TXQ_UNLOCK(txq);

	count = 0;
	do {
		/* Spin for 100ms. */
		DELAY(100000);

		if (txq->flush_state != SFXGE_FLUSH_PENDING)
			break;
	} while (++count < 20);

	SFXGE_EVQ_LOCK(evq);
	SFXGE_TXQ_LOCK(txq);

	KASSERT(txq->flush_state != SFXGE_FLUSH_FAILED,
	    ("txq->flush_state == SFXGE_FLUSH_FAILED"));

	txq->flush_state = SFXGE_FLUSH_DONE;

	txq->blocked = 0;
	txq->pending = txq->added;

	sfxge_tx_qcomplete(txq, evq);
	KASSERT(txq->completed == txq->added,
	    ("txq->completed != txq->added"));

	sfxge_tx_qreap(txq);
	KASSERT(txq->reaped == txq->completed,
	    ("txq->reaped != txq->completed"));

	txq->added = 0;
	txq->pending = 0;
	txq->completed = 0;
	txq->reaped = 0;

	/* Destroy the common code transmit queue. */
	efx_tx_qdestroy(txq->common);
	txq->common = NULL;

	efx_sram_buf_tbl_clear(sc->enp, txq->buf_base_id,
	    EFX_TXQ_NBUFS(sc->txq_entries));

	SFXGE_EVQ_UNLOCK(evq);
	SFXGE_TXQ_UNLOCK(txq);
}

static int
sfxge_tx_qstart(struct sfxge_softc *sc, unsigned int index)
{
	struct sfxge_txq *txq;
	efsys_mem_t *esmp;
	uint16_t flags;
	struct sfxge_evq *evq;
	int rc;

	txq = sc->txq[index];
	esmp = &txq->mem;
	evq = sc->evq[txq->evq_index];

	KASSERT(txq->init_state == SFXGE_TXQ_INITIALIZED,
	    ("txq->init_state != SFXGE_TXQ_INITIALIZED"));
	KASSERT(evq->init_state == SFXGE_EVQ_STARTED,
	    ("evq->init_state != SFXGE_EVQ_STARTED"));

	/* Program the buffer table. */
	if ((rc = efx_sram_buf_tbl_set(sc->enp, txq->buf_base_id, esmp,
	    EFX_TXQ_NBUFS(sc->txq_entries))) != 0)
		return (rc);

	/* Determine the kind of queue we are creating. */
	switch (txq->type) {
	case SFXGE_TXQ_NON_CKSUM:
		flags = 0;
		break;
	case SFXGE_TXQ_IP_CKSUM:
		flags = EFX_CKSUM_IPV4;
		break;
	case SFXGE_TXQ_IP_TCP_UDP_CKSUM:
		flags = EFX_CKSUM_IPV4 | EFX_CKSUM_TCPUDP;
		break;
	default:
		KASSERT(0, ("Impossible TX queue"));
		flags = 0;
		break;
	}

	/* Create the common code transmit queue. */
	if ((rc = efx_tx_qcreate(sc->enp, index, txq->type, esmp,
	    sc->txq_entries, txq->buf_base_id, flags, evq->common,
	    &txq->common)) != 0)
		goto fail;

	SFXGE_TXQ_LOCK(txq);

	/* Enable the transmit queue. */
	efx_tx_qenable(txq->common);

	txq->init_state = SFXGE_TXQ_STARTED;

	SFXGE_TXQ_UNLOCK(txq);

	return (0);

fail:
	efx_sram_buf_tbl_clear(sc->enp, txq->buf_base_id,
	    EFX_TXQ_NBUFS(sc->txq_entries));
	return (rc);
}

void
sfxge_tx_stop(struct sfxge_softc *sc)
{
	int index;

	index = sc->txq_count;
	while (--index >= 0)
		sfxge_tx_qstop(sc, index);

	/* Tear down the transmit module */
	efx_tx_fini(sc->enp);
}

int
sfxge_tx_start(struct sfxge_softc *sc)
{
	int index;
	int rc;

	/* Initialize the common code transmit module. */
	if ((rc = efx_tx_init(sc->enp)) != 0)
		return (rc);

	for (index = 0; index < sc->txq_count; index++) {
		if ((rc = sfxge_tx_qstart(sc, index)) != 0)
			goto fail;
	}

	return (0);

fail:
	while (--index >= 0)
		sfxge_tx_qstop(sc, index);

	efx_tx_fini(sc->enp);

	return (rc);
}

/**
 * Destroy a transmit queue.
 */
static void
sfxge_tx_qfini(struct sfxge_softc *sc, unsigned int index)
{
	struct sfxge_txq *txq;
	unsigned int nmaps;

	txq = sc->txq[index];

	KASSERT(txq->init_state == SFXGE_TXQ_INITIALIZED,
	    ("txq->init_state != SFXGE_TXQ_INITIALIZED"));

	if (txq->type == SFXGE_TXQ_IP_TCP_UDP_CKSUM)
		tso_fini(txq);

	/* Free the context arrays. */
	free(txq->pend_desc, M_SFXGE);
	nmaps = sc->txq_entries;
	while (nmaps-- != 0)
		bus_dmamap_destroy(txq->packet_dma_tag, txq->stmp[nmaps].map);
	free(txq->stmp, M_SFXGE);

	/* Release DMA memory mapping. */
	sfxge_dma_free(&txq->mem);

	sc->txq[index] = NULL;

#ifdef SFXGE_HAVE_MQ
	SFXGE_TXQ_LOCK_DESTROY(txq);
#endif

	free(txq, M_SFXGE);
}

static int
sfxge_tx_qinit(struct sfxge_softc *sc, unsigned int txq_index,
    enum sfxge_txq_type type, unsigned int evq_index)
{
	char name[16];
	struct sysctl_oid *txq_node;
	struct sfxge_txq *txq;
	struct sfxge_evq *evq;
#ifdef SFXGE_HAVE_MQ
	struct sfxge_tx_dpl *stdp;
#endif
	efsys_mem_t *esmp;
	unsigned int nmaps;
	int rc;

	txq = malloc(sizeof(struct sfxge_txq), M_SFXGE, M_ZERO | M_WAITOK);
	txq->sc = sc;
	txq->entries = sc->txq_entries;
	txq->ptr_mask = txq->entries - 1;

	sc->txq[txq_index] = txq;
	esmp = &txq->mem;

	evq = sc->evq[evq_index];

	/* Allocate and zero DMA space for the descriptor ring. */
	if ((rc = sfxge_dma_alloc(sc, EFX_TXQ_SIZE(sc->txq_entries), esmp)) != 0)
		return (rc);
	(void)memset(esmp->esm_base, 0, EFX_TXQ_SIZE(sc->txq_entries));

	/* Allocate buffer table entries. */
	sfxge_sram_buf_tbl_alloc(sc, EFX_TXQ_NBUFS(sc->txq_entries),
				 &txq->buf_base_id);

	/* Create a DMA tag for packet mappings. */
	if (bus_dma_tag_create(sc->parent_dma_tag, 1, 0x1000,
	    MIN(0x3FFFFFFFFFFFUL, BUS_SPACE_MAXADDR), BUS_SPACE_MAXADDR, NULL,
	    NULL, 0x11000, SFXGE_TX_MAPPING_MAX_SEG, 0x1000, 0, NULL, NULL,
	    &txq->packet_dma_tag) != 0) {
		device_printf(sc->dev, "Couldn't allocate txq DMA tag\n");
		rc = ENOMEM;
		goto fail;
	}

	/* Allocate pending descriptor array for batching writes. */
	txq->pend_desc = malloc(sizeof(efx_buffer_t) * sc->txq_entries,
				M_SFXGE, M_ZERO | M_WAITOK);

	/* Allocate and initialise mbuf DMA mapping array. */
	txq->stmp = malloc(sizeof(struct sfxge_tx_mapping) * sc->txq_entries,
	    M_SFXGE, M_ZERO | M_WAITOK);
	for (nmaps = 0; nmaps < sc->txq_entries; nmaps++) {
		rc = bus_dmamap_create(txq->packet_dma_tag, 0,
				       &txq->stmp[nmaps].map);
		if (rc != 0)
			goto fail2;
	}

	snprintf(name, sizeof(name), "%u", txq_index);
	txq_node = SYSCTL_ADD_NODE(
		device_get_sysctl_ctx(sc->dev),
		SYSCTL_CHILDREN(sc->txqs_node),
		OID_AUTO, name, CTLFLAG_RD, NULL, "");
	if (txq_node == NULL) {
		rc = ENOMEM;
		goto fail_txq_node;
	}

	if (type == SFXGE_TXQ_IP_TCP_UDP_CKSUM &&
	    (rc = tso_init(txq)) != 0)
		goto fail3;

#ifdef SFXGE_HAVE_MQ
	if (sfxge_tx_dpl_get_max <= 0) {
		log(LOG_ERR, "%s=%d must be greater than 0",
		    SFXGE_PARAM_TX_DPL_GET_MAX, sfxge_tx_dpl_get_max);
		rc = EINVAL;
		goto fail_tx_dpl_get_max;
	}
	if (sfxge_tx_dpl_get_non_tcp_max <= 0) {
		log(LOG_ERR, "%s=%d must be greater than 0",
		    SFXGE_PARAM_TX_DPL_GET_NON_TCP_MAX,
		    sfxge_tx_dpl_get_non_tcp_max);
		rc = EINVAL;
		goto fail_tx_dpl_get_max;
	}
	if (sfxge_tx_dpl_put_max < 0) {
		log(LOG_ERR, "%s=%d must be greater or equal to 0",
		    SFXGE_PARAM_TX_DPL_PUT_MAX, sfxge_tx_dpl_put_max);
		rc = EINVAL;
		goto fail_tx_dpl_put_max;
	}

	/* Initialize the deferred packet list. */
	stdp = &txq->dpl;
	stdp->std_put_max = sfxge_tx_dpl_put_max;
	stdp->std_get_max = sfxge_tx_dpl_get_max;
	stdp->std_get_non_tcp_max = sfxge_tx_dpl_get_non_tcp_max;
	stdp->std_getp = &stdp->std_get;

	SFXGE_TXQ_LOCK_INIT(txq, device_get_nameunit(sc->dev), txq_index);

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(sc->dev),
			SYSCTL_CHILDREN(txq_node), OID_AUTO,
			"dpl_get_count", CTLFLAG_RD | CTLFLAG_STATS,
			&stdp->std_get_count, 0, "");
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(sc->dev),
			SYSCTL_CHILDREN(txq_node), OID_AUTO,
			"dpl_get_non_tcp_count", CTLFLAG_RD | CTLFLAG_STATS,
			&stdp->std_get_non_tcp_count, 0, "");
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(sc->dev),
			SYSCTL_CHILDREN(txq_node), OID_AUTO,
			"dpl_get_hiwat", CTLFLAG_RD | CTLFLAG_STATS,
			&stdp->std_get_hiwat, 0, "");
#endif

	txq->type = type;
	txq->evq_index = evq_index;
	txq->txq_index = txq_index;
	txq->init_state = SFXGE_TXQ_INITIALIZED;

	return (0);

fail_tx_dpl_put_max:
fail_tx_dpl_get_max:
fail3:
fail_txq_node:
	free(txq->pend_desc, M_SFXGE);
fail2:
	while (nmaps-- != 0)
		bus_dmamap_destroy(txq->packet_dma_tag, txq->stmp[nmaps].map);
	free(txq->stmp, M_SFXGE);
	bus_dma_tag_destroy(txq->packet_dma_tag);

fail:
	sfxge_dma_free(esmp);

	return (rc);
}

static const struct {
	const char *name;
	size_t offset;
} sfxge_tx_stats[] = {
#define	SFXGE_TX_STAT(name, member) \
	{ #name, offsetof(struct sfxge_txq, member) }
	SFXGE_TX_STAT(tso_bursts, tso_bursts),
	SFXGE_TX_STAT(tso_packets, tso_packets),
	SFXGE_TX_STAT(tso_long_headers, tso_long_headers),
	SFXGE_TX_STAT(tso_pdrop_too_many, tso_pdrop_too_many),
	SFXGE_TX_STAT(tso_pdrop_no_rsrc, tso_pdrop_no_rsrc),
	SFXGE_TX_STAT(tx_collapses, collapses),
	SFXGE_TX_STAT(tx_drops, drops),
	SFXGE_TX_STAT(tx_get_overflow, get_overflow),
	SFXGE_TX_STAT(tx_get_non_tcp_overflow, get_non_tcp_overflow),
	SFXGE_TX_STAT(tx_put_overflow, put_overflow),
	SFXGE_TX_STAT(tx_netdown_drops, netdown_drops),
};

static int
sfxge_tx_stat_handler(SYSCTL_HANDLER_ARGS)
{
	struct sfxge_softc *sc = arg1;
	unsigned int id = arg2;
	unsigned long sum;
	unsigned int index;

	/* Sum across all TX queues */
	sum = 0;
	for (index = 0; index < sc->txq_count; index++)
		sum += *(unsigned long *)((caddr_t)sc->txq[index] +
					  sfxge_tx_stats[id].offset);

	return (SYSCTL_OUT(req, &sum, sizeof(sum)));
}

static void
sfxge_tx_stat_init(struct sfxge_softc *sc)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(sc->dev);
	struct sysctl_oid_list *stat_list;
	unsigned int id;

	stat_list = SYSCTL_CHILDREN(sc->stats_node);

	for (id = 0;
	     id < sizeof(sfxge_tx_stats) / sizeof(sfxge_tx_stats[0]);
	     id++) {
		SYSCTL_ADD_PROC(
			ctx, stat_list,
			OID_AUTO, sfxge_tx_stats[id].name,
			CTLTYPE_ULONG|CTLFLAG_RD,
			sc, id, sfxge_tx_stat_handler, "LU",
			"");
	}
}

void
sfxge_tx_fini(struct sfxge_softc *sc)
{
	int index;

	index = sc->txq_count;
	while (--index >= 0)
		sfxge_tx_qfini(sc, index);

	sc->txq_count = 0;
}


int
sfxge_tx_init(struct sfxge_softc *sc)
{
	struct sfxge_intr *intr;
	int index;
	int rc;

	intr = &sc->intr;

	KASSERT(intr->state == SFXGE_INTR_INITIALIZED,
	    ("intr->state != SFXGE_INTR_INITIALIZED"));

#ifdef SFXGE_HAVE_MQ
	sc->txq_count = SFXGE_TXQ_NTYPES - 1 + sc->intr.n_alloc;
#else
	sc->txq_count = SFXGE_TXQ_NTYPES;
#endif

	sc->txqs_node = SYSCTL_ADD_NODE(
		device_get_sysctl_ctx(sc->dev),
		SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
		OID_AUTO, "txq", CTLFLAG_RD, NULL, "Tx queues");
	if (sc->txqs_node == NULL) {
		rc = ENOMEM;
		goto fail_txq_node;
	}

	/* Initialize the transmit queues */
	if ((rc = sfxge_tx_qinit(sc, SFXGE_TXQ_NON_CKSUM,
	    SFXGE_TXQ_NON_CKSUM, 0)) != 0)
		goto fail;

	if ((rc = sfxge_tx_qinit(sc, SFXGE_TXQ_IP_CKSUM,
	    SFXGE_TXQ_IP_CKSUM, 0)) != 0)
		goto fail2;

	for (index = 0;
	     index < sc->txq_count - SFXGE_TXQ_NTYPES + 1;
	     index++) {
		if ((rc = sfxge_tx_qinit(sc, SFXGE_TXQ_NTYPES - 1 + index,
		    SFXGE_TXQ_IP_TCP_UDP_CKSUM, index)) != 0)
			goto fail3;
	}

	sfxge_tx_stat_init(sc);

	return (0);

fail3:
	while (--index >= 0)
		sfxge_tx_qfini(sc, SFXGE_TXQ_IP_TCP_UDP_CKSUM + index);

	sfxge_tx_qfini(sc, SFXGE_TXQ_IP_CKSUM);

fail2:
	sfxge_tx_qfini(sc, SFXGE_TXQ_NON_CKSUM);

fail:
fail_txq_node:
	sc->txq_count = 0;
	return (rc);
}
