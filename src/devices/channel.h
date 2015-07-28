/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 214
 *  File name: channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <src/types/types.h>
#include <src/multiplexers/muxable.h>
#include <src/buffers/buffer.h>
#include "messages.h"
#include "features.h"



/**
 * Every CamIO channel must implement this interface, see function prototypes in api.h
 */
typedef struct camio_channel_interface_s{

    void (*destroy)(camio_channel_t* this);

    //Read operations
    camio_error_t (*read_buffer_request)(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io );
    camio_error_t (*read_buffer_result) (camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io );
    camio_error_t (*read_request)(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io );
    camio_error_t (*read_result) (camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io );
    camio_error_t (*read_buffer_release)(camio_channel_t* this, camio_buffer_t* buffer_o);

    //Write operations
    camio_error_t (*write_buffer_request)(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io );
    camio_error_t (*write_buffer_result) (camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);
    camio_error_t (*write_request)(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io );
    camio_error_t (*write_result) (camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);
    camio_error_t (*write_buffer_release)(camio_channel_t* this, camio_buffer_t* buffer_o);

} camio_channel_interface_t;

/**
 * This is a basic CamIO channel structure. All channels must implement this. Streams will use the macros provided to overload
 * this structure with and include a private data structure.
 */
typedef struct camio_channel_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer to a constant, but saving 6x8bytes seems a
     * little silly when it will cost a pointer dereference on the critical
     * path.
     */
    camio_channel_interface_t vtable;

    /**
     * Holds the meta-data structure describing the properties of this device.
     */
    camio_device_features_t features;


    /**
     * Holds the multiplexable structures for adding into a multiplexor
     */
    camio_muxable_t rd_muxable; //For reading
    camio_muxable_t rd_buff_muxable; //For read buffers
    camio_muxable_t wr_muxable; //For writing
    camio_muxable_t wr_buff_muxable; //For write buffers


} camio_channel_t;


#define CHANNEL_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_CHANNEL(name)\
        new_##name##_channel()

#define NEW_CHANNEL_DECLARE(NAME)\
        camio_channel_t* new_##NAME##_channel()

#define NEW_CHANNEL_DEFINE(NAME, PRIVATE_TYPE) \
    static const camio_channel_interface_t NAME##_channel_interface = {\
            .read_buffer_request    = NAME##_read_buffer_request,\
            .read_buffer_result     = NAME##_read_buffer_result,\
            .read_buffer_release    = NAME##_read_buffer_release,\
            .read_request           = NAME##_read_request,\
            .read_result            = NAME##_read_result,\
            \
            .write_buffer_request   = NAME##_write_buffer_request,\
            .write_buffer_result    = NAME##_write_buffer_result,\
            .write_buffer_release   = NAME##_write_buffer_release,\
            .write_request          = NAME##_write_request,\
            .write_result           = NAME##_write_result,\
            .destroy                = NAME##_destroy,\
    };\
    \
    NEW_CHANNEL_DECLARE(NAME)\
    {\
        camio_channel_t* result = (camio_channel_t*)calloc(1,sizeof(camio_channel_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        result->vtable                          = NAME##_channel_interface;\
        \
        result->rd_buff_muxable.mode            = CAMIO_MUX_MODE_READ_BUFF;\
        result->rd_buff_muxable.parent.channel  = result;\
        result->rd_buff_muxable.vtable.ready    = NAME##_read_buffer_ready;\
        result->rd_buff_muxable.fd              = -1;\
        \
        result->rd_muxable.mode                 = CAMIO_MUX_MODE_READ;\
        result->rd_muxable.parent.channel       = result;\
        result->rd_muxable.vtable.ready         = NAME##_read_ready;\
        result->rd_muxable.fd                   = -1;\
        \
        result->wr_muxable.mode                 = CAMIO_MUX_MODE_WRITE;\
        result->wr_muxable.parent.channel       = result;\
        result->wr_muxable.vtable.ready         = NAME##_write_ready;\
        result->wr_muxable.fd                   = -1;\
        \
        result->wr_buff_muxable.mode            = CAMIO_MUX_MODE_WRITE_BUFF;\
        result->wr_buff_muxable.parent.channel  = result;\
        result->wr_buff_muxable.vtable.ready    = NAME##_write_buffer_ready;\
        result->wr_buff_muxable.fd              = -1;\
        \
        return result;\
    }



#endif /* CHANNEL_H_ */
