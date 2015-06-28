/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 17, 2014
 *  File name: api.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


/*
 * CamIO - The Cambridge Input/Output API
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details.
 *
 *  Created:   Aug 15, 2014
 *  File name: api.c
 *  Description:
 *  <INSERT DESCRIPTION HERE>
 */


#include <src/transports/connector.h>
#include <src/transports/features.h>
#include <src/transports/stream.h>
#include <src/errors/camio_errors.h>
#include "../types/types.h"
#include "../buffers/buffer.h"
#include "../camio_debug.h"
#include "api.h"


inline camio_error_t camio_connect( camio_connector_t* this, camio_stream_t** stream_o )
{
    return this->vtable.connect(this, stream_o);
}


inline void camio_connector_destroy(camio_connector_t* this)
{
    this->vtable.destroy(this);
}

inline camio_error_t camio_connector_ready( camio_connector_t* this)
{
    return this->muxable.vtable.ready(&this->muxable);
}


inline camio_error_t camio_read_request( camio_stream_t* this,  ch_word buffer_offset, ch_word source_offset)
{
    return this->vtable.read_request(this, buffer_offset, source_offset);
}


inline camio_error_t camio_read_ready( camio_stream_t* this)
{
    return this->rd_muxable.vtable.ready(&this->rd_muxable);
}


inline camio_error_t camio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
{
    return this->vtable.read_acquire(this, buffer_o);
}


inline camio_error_t camio_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    return this->vtable.read_release(this, buffer);
}


inline camio_error_t camio_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    return this->vtable.write_acquire(this, buffer_o);
}


inline camio_error_t camio_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain )
{
    return this->vtable.write_commit(this, buffer_chain);
}


inline camio_error_t camio_write_ready( camio_stream_t* this)
{
    return this->wr_muxable.vtable.ready(&this->wr_muxable);
}


inline camio_error_t camio_write_release(camio_stream_t* this, camio_wr_buffer_t** buffers_chain)
{
    return this->vtable.write_release(this, buffers_chain);
}


inline void camio_stream_destroy(camio_stream_t* this)
{
    this->vtable.destroy(this);
}



inline camio_error_t camio_copy_rw(camio_wr_buffer_t** wr_buffer, camio_rd_buffer_t** rd_buffer, ch_word offset, ch_word copy_len)
{
    (void)offset;

    //TODO XXX this is temporary until the actual copy function is implemented
    DBG("Copying %lli bytes from %p to %p\n",(*rd_buffer)->data_len,(*rd_buffer)->data_start, (*wr_buffer)->buffer_start);
    memcpy( (*wr_buffer)->buffer_start, (*rd_buffer)->data_start, copy_len);
    (*wr_buffer)->data_start = (*wr_buffer)->buffer_start;
    (*wr_buffer)->data_len = (*rd_buffer)->data_len;
    DBG("Done Copying %lli bytes from %p to %p\n",(*rd_buffer)->data_len,(*rd_buffer)->data_start, (*wr_buffer)->buffer_start);

    return CAMIO_ENOERROR;
}


inline camio_error_t camio_set_opts(camio_wr_buffer_t** wr_buffer,  ch_word buffer_offset, ch_word dest_offset)
{
    (void)wr_buffer;
    (void)buffer_offset;
    (void)dest_offset;
    return CAMIO_NOTIMPLEMENTED;
}


inline camio_error_t camio_chain(camio_wr_buffer_t** buffer_chain_head, camio_wr_buffer_t** buffer )
{
    (void)buffer_chain_head;
    (void)buffer;
    return CAMIO_NOTIMPLEMENTED;
}


inline camio_error_t camio_mux_insert(camio_mux_t* this, camio_muxable_t* muxable, ch_word id)
{
    return this->vtable.insert(this,muxable,id);
}


inline camio_error_t camio_mux_remove(camio_mux_t* this, camio_muxable_t* muxable)
{
    return this->vtable.remove(this,muxable);
}


inline camio_error_t camio_mux_select(camio_mux_t* this, /*struct timespec timeout,*/ camio_muxable_t** muxable_o)
{
    return this->vtable.select(this, /*timeout,*/ muxable_o);
}


inline void camio_mux_destroy(camio_mux_t* this)
{
    this->vtable.destroy(this);
}




