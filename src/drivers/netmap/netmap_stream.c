//#CFLAGS=-Wno-unused-parameter
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

typedef struct netmap_stream_priv_s {
    int netmap_fd;
    struct netmap_if* netmap_if;
} netmap_stream_priv_t;



static camio_error_t netmap_construct(camio_stream_t* this)
{
    (void)this;
    return CAMIO_NOTIMPLEMENTED;
}

static void netmap_destroy(camio_stream_t* this)
{
    (void)this;
}



static camio_error_t netmap_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o, ch_word buffer_offset,
          ch_word source_offset)
{
    (void)this;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_read_release(camio_stream_t* this, camio_rd_buffer_t* buffer_chain)
{
    (void)this;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    (void)this;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_commit(camio_stream_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
       ch_word dest_offset)
{
    (void)this;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t netmap_write_release(camio_stream_t* this, camio_wr_buffer_t* buffers_chain)
{
    (void)this;
    return CAMIO_NOTIMPLEMENTED;
}




NEW_STREAM_DEFINE(netmap,netmap_stream_priv_t)


