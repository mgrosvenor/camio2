/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include <src/transports/stream.h>

#include "../../camio_debug.h"

#include "udp_stream.h"
#include "udp_buffer.h"

typedef struct udp_stream_priv_s {
    ch_word udp_fd;
} udp_stream_priv_t;


camio_error_t udp_stream_construct(camio_stream_t* this, ch_word udp_fd, struct udp_if* net_if, ch_word rings_start,
        ch_word rings_end)
{
    DBG("rings_start=%i,rings_end=%i\n", rings_start, rings_end);

    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    priv->udp_fd     = udp_fd;


    return CAMIO_ENOERROR;
}


static void udp_destroy(camio_stream_t* this)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)this;
}


static camio_error_t udp_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o,  ch_word* chain_len_o,
        ch_word buffer_offset, ch_word source_offset)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    (void)buffer_offset; //Not supported
    (void)source_offset; //Not supported
    (void)buffer_chain_o;

    DBG("Doing read acquire\n");


    //There were no packets to receive, please come back later
    return CAMIO_ETRYAGAIN;
}


static camio_error_t udp_read_release(camio_stream_t* this, camio_rd_buffer_t* buffer_chain)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    //Walk to the end of the chain,
    camio_rd_buffer_t* buffer_curr = buffer_chain;
    camio_rd_buffer_t* buffer_prev = NULL;
    while(buffer_curr->next){
        buffer_prev = buffer_curr;
        buffer_curr = buffer_curr->next;
        // TODO: make sure slot indexes are monotonically increasing
    }

    udp_buffer_priv_t* buff_priv = BUFFER_GET_PRIVATE(buffer_curr);
    ch_word ring_idx = buff_priv->ring_idx;
    ch_word slot_idx = buff_priv->slot_idx;

    struct udp_ring* ring = priv->rx_rings[ring_idx];
    //Advance the head and current pointer, freeing the buffers to get reused
    ring->cur = slot_idx;
    ring->head = slot_idx;

    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    (void)this;
    (void)buffer_chain_o;
    (void)count_io;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_commit(camio_stream_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
        ch_word dest_offset)
{
    (void)this;
    (void)buffer_chain;
    (void)buffer_offset;
    (void)dest_offset;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_release(camio_stream_t* this, camio_wr_buffer_t* buffers_chain)
{
    (void)this;
    (void)buffers_chain;
    return CAMIO_NOTIMPLEMENTED;
}


NEW_STREAM_DEFINE(udp,udp_stream_priv_t)
