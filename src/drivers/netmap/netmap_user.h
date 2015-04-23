/*
 * Copyright (C) 2011-2014 Universita` di Pisa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $FreeBSD$
 *
 * Functions and macros to manipulate netmap structures and packets
 * in userspace. See netmap(4) for more information.
 *
 * The address of the struct netmap_if, say nifp, is computed from the
 * value returned from ioctl(.., NIOCREG, ...) and the mmap region:
 *	ioctl(fd, NIOCREG, &req);
 *	mem = mmap(0, ... );
 *	nifp = NETMAP_IF(mem, req.nr_nifp);
 *		(so simple, we could just do it manually)
 *
 * From there:
 *	struct netmap_ring *NETMAP_TXRING(nifp, index)
 *	struct netmap_ring *NETMAP_RXRING(nifp, index)
 *		we can access ring->nr_cur, ring->nr_avail, ring->nr_flags
 *
 *	ring->slot[i] gives us the i-th slot (we can access
 *		directly len, flags, buf_idx)
 *
 *	char *buf = NETMAP_BUF(ring, x) returns a pointer to
 *		the buffer numbered x
 *
 * All ring indexes (head, cur, tail) should always move forward.
 * To compute the next index in a circular ring you can use
 *	i = nm_ring_next(ring, i);
 *
 * To ease porting apps from pcap to netmap we supply a few fuctions
 * that can be called to open, close, read and write on netmap in a way
 * similar to libpcap. Note that the read/write function depend on
 * an ioctl()/select()/poll() being issued to refill rings or push
 * packets out.
 *
 * In order to use these, include #define NETMAP_WITH_LIBS
 * in the source file that invokes these functions.
 */

#ifndef _NET_NETMAP_USER_H_
#define _NET_NETMAP_USER_H_

#include <stdint.h>
#include <sys/socket.h>		/* apple needs sockaddr */
#include <net/if.h>		/* IFNAMSIZ */

#ifndef likely
#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#endif /* likely and unlikely */

#include "netmap.h"

/* helper macro */
#define _NETMAP_OFFSET(type, ptr, offset) \
        ((type)(void *)((char *)(ptr) + (offset)))

#define NETMAP_IF(_base, _ofs)	_NETMAP_OFFSET(struct netmap_if *, _base, _ofs)

#define NETMAP_TXRING(nifp, index) _NETMAP_OFFSET(struct netmap_ring *, \
        nifp, (nifp)->ring_ofs[index] )

#define NETMAP_RXRING(nifp, index) _NETMAP_OFFSET(struct netmap_ring *,	\
        nifp, (nifp)->ring_ofs[index + (nifp)->ni_tx_rings + 1] )

#define NETMAP_BUF(ring, index)				\
        ((char *)(ring) + (ring)->buf_ofs + ((index)*(ring)->nr_buf_size))

#define NETMAP_BUF_IDX(ring, buf)			\
        ( ((char *)(buf) - ((char *)(ring) + (ring)->buf_ofs) ) / \
                (ring)->nr_buf_size )


static inline uint32_t
nm_ring_next(struct netmap_ring *r, uint32_t i)
{
    return ( unlikely(i + 1 == r->num_slots) ? 0 : i + 1);
}


/*
 * Return 1 if we have pending transmissions in the tx ring.
 * When everything is complete ring->head = ring->tail + 1 (modulo ring size)
 */
static inline int
nm_tx_pending(struct netmap_ring *r)
{
    return nm_ring_next(r, r->tail) != r->head;
}


static inline uint32_t
nm_ring_space(struct netmap_ring *ring)
{
    int ret = ring->tail - ring->cur;
    if (ret < 0)
        ret += ring->num_slots;
    return ret;
}



#endif /* _NET_NETMAP_USER_H_ */
