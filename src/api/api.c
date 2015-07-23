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

//This switches of the sanity checks in release mode.
#ifndef NDEBUG
#define DO_SANITY_CHECKS
#endif

#include <src/devices/controller.h>
#include <src/devices/features.h>
#include <src/devices/channel.h>
#include <src/errors/camio_errors.h>
#include "../types/types.h"
#include "../buffers/buffer.h"
#include "../camio_debug.h"
#include "api.h"
#include <deps/chaste/utils/util.h>

#define CHECK(predicate, fail_message)\
    if(unlikely(predicate)){\
        ERR(fail_message);\
        return CAMIO_EINVALID;\
    }

#define CHECK_NR(predicate, fail_message)\
    if(unlikely(predicate)){\
        ERR(fail_message);\
        return;\
    }



camio_error_t camio_chan_rd_req( camio_channel_t* this, camio_rd_req_t* req_vec, ch_word vec_len )
{
    #ifdef DO_SANITY_CHECKS
        CHECK(NULL == this,"This is null???\n")
        CHECK(NULL == req_vec,"Supplied read request vector is NULL\n")
        CHECK( 0 >= vec_len,"Supplied read request vector is empty\n")
    #endif

    return this->vtable.read_request(this, req_vec, vec_len);
}


camio_error_t camio_chan_rd_ready( camio_channel_t* this)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( this->rd_muxable.mode != CAMIO_MUX_MODE_READ, "Wrong kind of muxable mode! Expected CAMIO_MUX_MODE_READ\n")
    #endif

    return this->rd_muxable.vtable.ready(&this->rd_muxable);
}

camio_error_t camio_chan_rd_res( camio_channel_t* this, camio_rd_req_t** res_o)
{
    #ifdef DO_SANITY_CHECKS //Only apply these checks in debug mode. Keep the speed when we need it?
        CHECK( NULL == this,"This is null???\n")
        CHECK( NULL == res_o,"Request result pointer supplied is NULL\n")
    #endif

    return this->vtable.read_result(this, res_o);
}


camio_error_t camio_chan_rd_release(camio_channel_t* this, camio_rd_buffer_t* buffer)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( NULL == buffer,"Buffer pointer supplied is NULL\n")
        CHECK(buffer->__internal.__parent != this, "Cannot release a buffer that does not belong to us!\n");
    #endif


    return this->vtable.read_release(this, buffer);
}


camio_error_t camio_chan_wr_buff_req(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( 0 >= vec_len,"Supplied read request vector is empty\n")
    #endif

    return this->vtable.write_buffer_request(this, req_vec, vec_len);
}

camio_error_t camio_chan_wr_buff_ready(camio_channel_t* this)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( NULL == this,"This is null???\n")
        CHECK( this->rd_muxable.mode != CAMIO_MUX_MODE_WRITE_BUFF,
               "Wrong kind of muxable mode. Expected CAMIO_MUX_MODE_WRITE_BUFF !\n")
    #endif

    return this->wr_buff_muxable.vtable.ready(&this->wr_buff_muxable);
}


camio_error_t camio_chan_wr_buff_res(camio_channel_t* this, camio_wr_req_t** req_o)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
    #endif

    return this->vtable.write_buffer_result(this, req_o);
}

camio_error_t camio_chan_wr_req(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( 0 >= vec_len,"Supplied read request vector is empty\n")
    #endif

    return this->vtable.write_request(this, req_vec, vec_len);
}

camio_error_t camio_chan_wr_ready( camio_channel_t* this)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( this->rd_muxable.mode != CAMIO_MUX_MODE_WRITE,"Wrong kind of muxable mode! Expected CAMIO_MUX_MODE_WRITE\n")
    #endif

    return this->wr_muxable.vtable.ready(&this->wr_muxable);
}


camio_error_t camio_chan_wr_res(camio_channel_t* this, camio_wr_req_t** res_o)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( NULL == res_o,"Request result pointer supplied is NULL\n")
    #endif

    return this->vtable.write_result(this, res_o);
}


camio_error_t camio_chan_wr_buff_release(camio_channel_t* this, camio_wr_buffer_t* buffer)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
        CHECK( NULL == buffer,"Buffer pointer supplied is NULL\n")
        CHECK(buffer->__internal.__parent != this,"Cannot release a buffer that does not belong to us!\n")
    #endif

    return this->vtable.write_buffer_release(this, buffer);
}


void camio_channel_destroy(camio_channel_t* this)
{
    #ifdef DO_SANITY_CHECKS
        CHECK_NR( NULL == this,"This is null???\n")
    #endif

    this->vtable.destroy(this);
}



inline camio_error_t camio_mux_insert(camio_mux_t* this, camio_muxable_t* muxable, ch_word id)
{
    return this->vtable.insert(this,muxable,id);
}


inline camio_error_t camio_mux_remove(camio_mux_t* this, camio_muxable_t* muxable)
{
    return this->vtable.remove(this,muxable);
}


inline camio_error_t camio_mux_select(camio_mux_t* this, camio_muxable_t** muxable_o, ch_word* which_o)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
    #endif

    return this->vtable.select(this, /*timeout,*/ muxable_o, which_o);
}

ch_word camio_mux_count(camio_mux_t* this)
{
    #ifdef DO_SANITY_CHECKS
        CHECK( NULL == this,"This is null???\n")
    #endif

    return this->vtable.count(this);
}


inline void camio_mux_destroy(camio_mux_t* this)
{
    #ifdef DO_SANITY_CHECKS
        CHECK_NR( NULL == this,"This is null???\n")
    #endif

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



