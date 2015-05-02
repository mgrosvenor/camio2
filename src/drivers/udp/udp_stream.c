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


camio_error_t udp_stream_construct(camio_stream_t* this, int udp_fd)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;
    (void)udp_fd;

    return CAMIO_ENOERROR;
}


static void udp_destroy(camio_stream_t* this)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;
    (void)this;
}


static camio_error_t udp_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o,  ch_word* chain_len_o,
        ch_word buffer_offset, ch_word source_offset)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    (void)buffer_offset; //Not supported
    (void)source_offset; //Not supported
    (void)buffer_chain_o;
    (void)chain_len_o;

    DBG("Doing read acquire\n");


    //There were no packets to receive, please come back later
    return CAMIO_ETRYAGAIN;
}


static camio_error_t udp_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer_chain)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;
    (void)buffer_chain;

    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    (void)this;
    (void)buffer_chain_o;
    (void)count_io;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain, ch_word buffer_offset,
        ch_word dest_offset)
{
    (void)this;
    (void)buffer_chain;
    (void)buffer_offset;
    (void)dest_offset;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_release(camio_stream_t* this, camio_wr_buffer_t** buffers_chain)
{
    (void)this;
    (void)buffers_chain;
    return CAMIO_NOTIMPLEMENTED;
}


NEW_STREAM_DEFINE(udp,udp_stream_priv_t)
