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


inline camio_error_t camio_read_request(camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len )
{
    return this->vtable.read_request(this, req_vec, req_vec_len);
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


inline camio_error_t camio_write_request(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len)
{
    return this->vtable.write_request(this, req_vec, req_vec_len);
}


inline camio_error_t camio_write_ready( camio_stream_t* this)
{
    return this->wr_muxable.vtable.ready(&this->wr_muxable);
}


inline camio_error_t camio_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer)
{
    return this->vtable.write_release(this, buffer);
}


inline void camio_stream_destroy(camio_stream_t* this)
{
    this->vtable.destroy(this);
}



inline camio_error_t camio_copy_rw(
        camio_wr_buffer_t** wr_buffer,
        camio_rd_buffer_t** rd_buffer,
        ch_word dst_offset,
        ch_word src_offset,
        ch_word copy_len
        )
{
    //TODO XXX add support for this
    if(dst_offset || src_offset){
        DBG("This feature is not yet supported\n");
        return CAMIO_NOTIMPLEMENTED;
    }

    camio_wr_buffer_t* wr_buff =  *wr_buffer;
    camio_wr_buffer_t* rd_buff =  *rd_buffer;


    //TODO XXX this is temporary until the actual copy function is implemented. The real copy should check if the source and
    //         dst buffers are actually different taking into account bindings etc. Check that the buffers are real, and have
    //         real memory associated with them etc etc.
    DBG("Copying %lli bytes from %p to %p\n",rd_buff->data_len,rd_buff->data_start, wr_buff->__internal.__mem_start);
    memcpy( wr_buff->__internal.__mem_start, rd_buff->data_start, copy_len);
    wr_buff->data_start = wr_buff->__internal.__mem_start;
    wr_buff->data_len   = copy_len;
    DBG("Done copying\n");

    return CAMIO_ENOERROR;
}


inline camio_error_t camio_mux_insert(camio_mux_t* this, camio_muxable_t* muxable, ch_word id)
{
    return this->vtable.insert(this,muxable,id);
}


inline camio_error_t camio_mux_remove(camio_mux_t* this, camio_muxable_t* muxable)
{
    return this->vtable.remove(this,muxable);
}


inline camio_error_t camio_mux_select(camio_mux_t* this,
        /*struct timespec timeout,*/
        camio_muxable_t** muxable_o,
        ch_word* which_o)
{
    return this->vtable.select(this, /*timeout,*/ muxable_o, which_o);
}

ch_word camio_mux_count(camio_mux_t* this)
{
    return this->vtable.count(this);
}


inline void camio_mux_destroy(camio_mux_t* this)
{
    this->vtable.destroy(this);
}




