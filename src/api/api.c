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


#include <src/devices/controller.h>
#include <src/devices/features.h>
#include <src/devices/channel.h>
#include <src/errors/camio_errors.h>
#include "../types/types.h"
#include "../buffers/buffer.h"
#include "../camio_debug.h"
#include "api.h"

camio_error_t camio_chan_rd_req( camio_channel_t* this, camio_rd_req_t* req_vec, ch_word vec_len )
{
    return this->vtable.read_request(this, req_vec, vec_len);
}


camio_error_t camio_chan_rd_ready( camio_channel_t* this)
{
    return this->rd_muxable.vtable.ready(&this->rd_muxable);
}

camio_error_t camio_chan_rd_res( camio_channel_t* this, camio_rd_req_t** req_o)

{
    return this->vtable.read_result(this, req_o);
}


camio_error_t camio_chan_rd_rel(camio_channel_t* this, camio_rd_buffer_t** buffer)
{
    return this->vtable.read_release(this, buffer);
}


camio_error_t camio_chan_wr_buff_req(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len)
{
    return this->vtable.write_buffer_request(this, req_vec, vec_len);
}

camio_error_t camio_chan_wr_buff_ready(camio_channel_t* this)
{
    return this->wr_buff_muxable.vtable.ready(&this->wr_buff_muxable);
}


camio_error_t camio_chan_wr_buff_res(camio_channel_t* this, camio_wr_req_t** req_o)
{
    return this->vtable.write_buffer_result(this, req_o);
}

camio_error_t camio_chan_wr_req(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len)
{
    return this->vtable.write_request(this, req_vec, vec_len);
}

camio_error_t camio_write_ready( camio_channel_t* this)
{
    return this->wr_muxable.vtable.ready(&this->wr_muxable);
}


camio_error_t camio_chan_wr_res(camio_channel_t* this, camio_wr_req_t** res_o)
{
    return this->vtable.write_result(this, res_o);
}


camio_error_t camio_chan_wr_buff_rel(camio_channel_t* this, camio_wr_buffer_t** buffer)
{
    return this->vtable.write_buffer_release(this, buffer);
}


void camio_channel_destroy(camio_channel_t* this)
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




