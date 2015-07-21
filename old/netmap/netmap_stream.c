
/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: netmap_channel.c
 *  Description:
 *  Implements the CamIO netmap channel
 */


#include <src/devices/channel.h>

#include "../../camio_debug.h"

#include "netmap_channel.h"
#include "netmap_user.h"
#include "netmap_buffer.h"

typedef struct netmap_channel_priv_s {
    ch_word netmap_fd;
    ch_word rings_start;
    ch_word rings_end;
    struct netmap_if* net_if;

    struct netmap_ring** tx_rings;
    struct netmap_ring** rx_rings;
    ch_word rings_count;
    ch_word tx_ring_current;
    ch_word rx_ring_current;

    camio_rd_buffer_t* rx_buffers;
    camio_wr_buffer_t* tx_buffers;
    ch_word slots_per_buff;
    ch_word total_slots;

} netmap_channel_priv_t;


camio_error_t netmap_channel_construct(camio_channel_t* this, ch_word netmap_fd, struct netmap_if* net_if, ch_word rings_start,
        ch_word rings_end)
{
    DBG("rings_start=%i,rings_end=%i\n", rings_start, rings_end);

    netmap_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    priv->netmap_fd     = netmap_fd;
    priv->net_if        = net_if;
    priv->rings_start   = rings_start;
    priv->rings_end     = rings_end;
    const ch_word rings_count = rings_end - rings_start;
    priv->rings_count   = rings_count;
    DBG("rings_total=%i\n", rings_count);


    priv->tx_rings = (struct netmap_ring**)calloc(rings_count,sizeof(struct netmap_ring*));
    priv->rx_rings = (struct netmap_ring**)calloc(rings_count,sizeof(struct netmap_ring*));

    if(priv->tx_rings == NULL){
        DBG("Could not allocate memory for netmap rings\n");
        return CAMIO_ENOMEM;
    }
    if(priv->rx_rings == NULL){
        DBG("Could not allocate memory for netmap rings\n");
        return CAMIO_ENOMEM;
    }

    for(ch_word ring = rings_start; ring < rings_end; ring++){
        priv->tx_rings[ring] = NETMAP_TXRING(net_if,ring);
        priv->rx_rings[ring] = NETMAP_RXRING(net_if,ring);
        DBG("Init %i tx_ring=%p,rx_ring=%p\n", ring, priv->tx_rings[ring], priv->rx_rings[ring]);
    }

    priv->slots_per_buff    = priv->tx_rings[0]->num_slots;
    priv->total_slots       = rings_count * priv->slots_per_buff;
    priv->tx_buffers        = CALLOC_BUFFER(netmap,priv->total_slots);
    priv->rx_buffers        = CALLOC_BUFFER(netmap,priv->total_slots);
    DBG("Total slots=%i\n", priv->total_slots);

    if(priv->tx_buffers == NULL){
        DBG("Could not allocate memory for TX buffers\n");
        return CAMIO_ENOMEM;
    }
    if(priv->rx_buffers == NULL){
        DBG("Could not allocate memory for TX buffers\n");
        return CAMIO_ENOMEM;
    }


    for(ch_word i = 0; i < priv->total_slots ; i++){
        const ch_word ring_idx = i / priv->slots_per_buff;
        const ch_word slot_idx = i % priv->slots_per_buff;
        //DBG("Idx=%i, Ring_idx=%i, buffidx=%i\n", i, ring_idx, buff_idx);

        const struct netmap_ring* rx_ring = priv->rx_rings[ring_idx];
        const struct netmap_ring* tx_ring = priv->tx_rings[ring_idx];

        priv->rx_buffers[i].timestamp_type              = CAMIO_BUFFER_TST_NONE;
        priv->rx_buffers[i].ts                          = NULL;
        priv->rx_buffers[i].buffer_len                  = rx_ring->nr_buf_size;
        priv->rx_buffers[i].buffer_start                = NETMAP_BUF(rx_ring,slot_idx);
        priv->rx_buffers[i].data_start                  = NULL;
        priv->rx_buffers[i].data_len                    = 0;
        priv->rx_buffers[i]._internal.__in_use          = false;
        priv->rx_buffers[i]._internal.__read_only       = true;
        priv->rx_buffers[i]._internal.__pool_id         = ring_idx;
        priv->rx_buffers[i]._internal.__buffer_id       = slot_idx;
        priv->rx_buffers[i]._internal.__buffer_parent   = this;
        netmap_buffer_priv_t* rx_buff_priv = BUFFER_GET_PRIVATE(priv->rx_buffers +i);
        rx_buff_priv->ring_idx = ring_idx;
        rx_buff_priv->slot_idx = slot_idx;

        priv->tx_buffers[i].timestamp_type              = CAMIO_BUFFER_TST_NONE;
        priv->tx_buffers[i].ts                          = NULL;
        priv->tx_buffers[i].buffer_len                  = tx_ring->nr_buf_size;
        priv->tx_buffers[i].buffer_start                = NETMAP_BUF(tx_ring,slot_idx);
        priv->tx_buffers[i].data_start                  = NULL;
        priv->tx_buffers[i].data_len                    = 0;
        priv->tx_buffers[i]._internal.__in_use          = false;
        priv->tx_buffers[i]._internal.__read_only       = false;
        priv->tx_buffers[i]._internal.__pool_id         = ring_idx;
        priv->tx_buffers[i]._internal.__buffer_id       = slot_idx;
        priv->tx_buffers[i]._internal.__buffer_parent   = this;
        netmap_buffer_priv_t* tx_buff_priv = BUFFER_GET_PRIVATE(priv->tx_buffers +i);
        tx_buff_priv->ring_idx = ring_idx;
        tx_buff_priv->slot_idx = slot_idx;

    }

    return CAMIO_ENOERROR;
}


static void netmap_destroy(camio_channel_t* this)
{
    netmap_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    if(priv->tx_buffers){
        free(priv->tx_buffers);
        priv->tx_buffers = NULL;
    }

    if(priv->rx_buffers){
        free(priv->rx_buffers);
        priv->rx_buffers = NULL;
    }

    if(priv->tx_rings){
        free(priv->tx_rings);
        priv->tx_rings = NULL;
    }

    if(priv->rx_rings){
        free(priv->rx_rings);
        priv->rx_rings = NULL;
    }

}

//From netmap(4) BSD 11-current
//http://www.freebsd.org/cgi/man.cgi?query=netmap&apropos=0&sektion=4&manpath=FreeBSD+11-current&arch=default&format=html
//
//RECEIVE RINGS
//On receive rings, after a netmap system call, the slots in the range head... tail-1 contain received packets.  User code
//should process them and advance head and cur past slots it wants to return to the kernel. cur may be moved further ahead if
//the user code wants to wait for more packets without returning all the previous slots to the kernel.
//
//At the next NIOCRXSYNC/select()/poll(), slots up to head-1 are returned to the kernel for further receives, and tail may
//advance to report new incoming packets. Below is an example of the evolution of an RX ring:
//
//after the syscall, there are some (h)eld and some (R)eceived slots
//   head  cur     tail
//    |     |       |
//    v     v       v
// RX  [..hhhhhhRRRRRRRR..........]
//
//user advances head and cur, releasing some slots and holding others
//       head cur  tail
//         |  |     |
//         v  v     v
// RX  [..*****hhhRRRRRR...........]
//
//NICRXSYNC/poll()/select() recovers slots and reports new packets
//       head cur        tail
//         |  |       |
//         v  v       v
// RX  [.......hhhRRRRRRRRRRRR....]


static camio_error_t netmap_read_acquire( camio_channel_t* this,  camio_rd_buffer_t** buffer_chain_o,  ch_word* chain_len_o,
        ch_word buffer_offset, ch_word source_offset)
{
    netmap_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    (void)buffer_offset; //Not supported
    (void)source_offset; //Not supported
    (void)buffer_chain_o;

    DBG("Doing read acquire\n");

    for(ch_word i = 0; i < priv->rings_count; i++){
        const ch_word ring_id = (priv->rx_ring_current + i) % priv->rings_count;
        struct netmap_ring* ring = priv->rx_rings[ring_id];
        const ch_word avail = nm_ring_space(ring);

        if(!avail){
            continue; //Nothing here, try another ring
        }
        DBG("There are %i new packets available\n", avail);

        if(chain_len_o){ *chain_len_o = avail; }
        camio_rd_buffer_t* current_buff = NULL;

        for(ch_word slot_idx = ring->cur; slot_idx < ring->cur + avail; slot_idx++){
            struct netmap_slot* slot = &ring->slot[slot_idx];
            current_buff            = priv->rx_buffers + slot_idx;
            current_buff->next      = priv->rx_buffers + slot_idx + 1;
            current_buff->data_len  = slot->len;

            //First slot is the output
            if(slot_idx == ring->cur){
                *buffer_chain_o = current_buff;
            }

            //Last slot has no next pointer
            if(slot_idx == ring->cur + avail - 1){
                current_buff->next = NULL;
            }
        }

        return CAMIO_ENOERROR;
    }

    //There were no packets to receive, please come back later
    return CAMIO_ETRYAGAIN;
}


static camio_error_t netmap_read_release(camio_channel_t* this, camio_rd_buffer_t* buffer_chain)
{
    netmap_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Walk to the end of the chain,
    camio_rd_buffer_t* buffer_curr = buffer_chain;
    camio_rd_buffer_t* buffer_prev = NULL;
    while(buffer_curr->next){
        buffer_prev = buffer_curr;
        buffer_curr = buffer_curr->next;
        // TODO: make sure slot indexes are monotonically increasing
    }

    netmap_buffer_priv_t* buff_priv = BUFFER_GET_PRIVATE(buffer_curr);
    ch_word ring_idx = buff_priv->ring_idx;
    ch_word slot_idx = buff_priv->slot_idx;

    struct netmap_ring* ring = priv->rx_rings[ring_idx];
    //Advance the head and current pointer, freeing the buffers to get reused
    ring->cur = slot_idx;
    ring->head = slot_idx;

    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_acquire(camio_channel_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    (void)this;
    (void)buffer_chain_o;
    (void)count_io;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_commit(camio_channel_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
        ch_word dest_offset)
{
    (void)this;
    (void)buffer_chain;
    (void)buffer_offset;
    (void)dest_offset;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_release(camio_channel_t* this, camio_wr_buffer_t* buffers_chain)
{
    (void)this;
    (void)buffers_chain;
    return CAMIO_NOTIMPLEMENTED;
}


NEW_CHANNEL_DEFINE(netmap,netmap_channel_priv_t)


