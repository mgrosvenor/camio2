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


#include "stream.h"


static camio_error_t read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o, ch_word buffer_offset,
          ch_word source_offset)
{
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t read_release(camio_stream_t* this, camio_rd_buffer_t* buffer_chain)
{
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t write_commit(camio_stream_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
       ch_word dest_offset)
{
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t write_release(camio_stream_t* this, camio_wr_buffer_t* buffers_chain)
{
    return CAMIO_NOTIMPLEMENTED;
}


static void stream_destroy(camio_stream_t* this)
{

}


const static camio_stream_interface_t stream_interface =
{
        .read_acquire   = read_acquire,
        .read_release   = read_release,
        .write_acquire  = write_acquire,
        .write_commit   = write_commit,
        .write_release  = write_release,
        .destory        = stream_destroy
 };

//static camio_stream_t stream;


