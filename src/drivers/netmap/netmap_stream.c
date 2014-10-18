
/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: netmap_stream.c
 *  Description:
 *  Implements the CamIO netmap stream
 */


#include <src/transports/stream.h>

#include "netmap_stream.h"
#include "netmap_user.h"
#include "../../camio_debug.h"

typedef struct netmap_stream_priv_s {
    ch_word netmap_fd;
    ch_word rings_start;
    ch_word rings_end;
    struct netmap_if* net_if;

    struct netmap_ring** tx_rings;
    struct netmap_ring** rx_rings;
    ch_word rings_count;

    camio_rd_buffer_t* rx_buffers;
    camio_wr_buffer_t* tx_buffers;
    ch_word slots_per_buff;
    ch_word total_slots;

} netmap_stream_priv_t;


camio_error_t netmap_stream_construct(camio_stream_t* this, ch_word netmap_fd, struct netmap_if* net_if, ch_word rings_start,
        ch_word rings_end)
{
    DBG("rings_start=%i,rings_end=%i\n", rings_start, rings_end);

    netmap_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    priv->netmap_fd     = netmap_fd;
    priv->net_if        = net_if;
    priv->rings_start   = rings_start;
    priv->rings_end     = rings_end;
    const ch_word rings_count = rings_end - rings_start;
    priv->rings_count   = rings_count;
    DBG("rings_total=%i\n", rings_count);


    priv->rx_buffers = calloc(rings_count,sizeof(camio_buffer_t));
    if(priv->rx_buffers == NULL){
        DBG("Could not allocate memory for RX buffers\n");
        return CAMIO_ENOMEM;
    }

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
    priv->tx_buffers        = calloc(priv->total_slots ,sizeof(camio_buffer_t));
    priv->rx_buffers        = calloc(priv->total_slots ,sizeof(camio_buffer_t));
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
        const ch_word buff_idx = i % priv->slots_per_buff;
        //DBG("Idx=%i, Ring_idx=%i, buffidx=%i\n", i, ring_idx, buff_idx);

        const struct netmap_ring* rx_ring = priv->rx_rings[ring_idx];
        const struct netmap_ring* tx_ring = priv->tx_rings[ring_idx];

        priv->rx_buffers[i].timestamp_type  = CAMIO_BUFFER_TST_NONE;
        priv->rx_buffers[i].ts              = NULL;
        priv->rx_buffers[i].buffer_len      = rx_ring->nr_buf_size;
        priv->rx_buffers[i].buffer_start    = NETMAP_BUF(rx_ring,buff_idx);
        priv->rx_buffers[i].data_start      = NULL;
        priv->rx_buffers[i].data_len        = 0;


        priv->tx_buffers[i].timestamp_type  = CAMIO_BUFFER_TST_NONE;
        priv->tx_buffers[i].ts              = NULL;
        priv->tx_buffers[i].buffer_len      = tx_ring->nr_buf_size;
        priv->tx_buffers[i].buffer_start    = NETMAP_BUF(tx_ring,buff_idx);
        priv->tx_buffers[i].data_start      = NULL;
        priv->tx_buffers[i].data_len        = 0;
    }

    return CAMIO_ENOERROR;
}


static void netmap_destroy(camio_stream_t* this)
{
    (void)this;
}



static camio_error_t netmap_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o, ch_word buffer_offset,
          ch_word source_offset)
{
    (void)this;
    (void)buffer_chain_o;
    (void)buffer_offset;
    (void)source_offset;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_read_release(camio_stream_t* this, camio_rd_buffer_t* buffer_chain)
{
    (void)this;
    (void)buffer_chain;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    (void)this;
    (void)buffer_chain_o;
    (void)count_io;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_commit(camio_stream_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
       ch_word dest_offset)
{
    (void)this;
    (void)buffer_chain;
    (void)buffer_offset;
    (void)dest_offset;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_release(camio_stream_t* this, camio_wr_buffer_t* buffers_chain)
{
    (void)this;
    (void)buffers_chain;
    return CAMIO_NOTIMPLEMENTED;
}


NEW_STREAM_DEFINE(netmap,netmap_stream_priv_t)


