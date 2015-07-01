/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 214
 *  File name: stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#ifndef STREAM_H_
#define STREAM_H_

#include <src/types/types.h>
#include <src/multiplexers/muxable.h>
#include <src/buffers/buffer.h>

#include "features.h"

#define CAMIO_READ_REQ_SIZE_ANY (-1)
#define CAMIO_READ_REQ_SRC_OFFSET_NONE (-1)
#define CAMIO_READ_REQ_DST_OFFSET_NONE (0)
typedef struct camio_read_req_s {
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
    ch_word read_size_hint;
} camio_read_req_t;

#define CAMIO_WRITE_REQ_DST_OFFSET_NONE (-1)
#define CAMIO_WRITE_REQ_SRC_OFFSET_NONE (0)
typedef struct camio_write_req_s {
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
    camio_wr_buffer_t* buffer;
} camio_write_req_t;


/**
 * Every CamIO stream must implement this interface, see function prototypes in api.h
 */
typedef struct camio_stream_interface_s{

    void (*destroy)(camio_stream_t* this);

    //Read operations
    camio_error_t (*read_request)(camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len );
    camio_error_t (*read_acquire)(camio_stream_t* this, camio_rd_buffer_t** buffer_o);
    camio_error_t (*read_release)(camio_stream_t* this, camio_wr_buffer_t** buffer_o);

    //Write operations
    camio_error_t (*write_acquire)(camio_stream_t* this, camio_wr_buffer_t** buffer_o);
    camio_error_t (*write_request)(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len);
    camio_error_t (*write_release)(camio_stream_t* this, camio_wr_buffer_t** buffer);

} camio_stream_interface_t;

/**
 * This is a basic CamIO stream structure. All streams must implement this. Streams will use the macros provided to overload
 * this structure with and include a private data structure.
 */
typedef struct camio_stream_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer to a constant, but saving 6x8bytes seems a
     * little silly when it will cost a pointer dereference on the critical
     * path.
     */
    camio_stream_interface_t vtable;

    /**
     * Holds the meta-data structure describing the properties of this transport.
     */
    camio_transport_features_t features;


    /**
     * Holds the multiplexable structures for adding into a multiplexor
     */
    camio_muxable_t rd_muxable; //For reading
    camio_muxable_t wr_muxable; //For writing


} camio_stream_t;


#define STREAM_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_STREAM(name)\
        new_##name##_stream()

#define NEW_STREAM_DECLARE(NAME)\
        camio_stream_t* new_##NAME##_stream()

#define NEW_STREAM_DEFINE(NAME, PRIVATE_TYPE) \
    const static camio_stream_interface_t NAME##_stream_interface = {\
            .read_request   = NAME##_read_request,\
            .read_acquire   = NAME##_read_acquire,\
            .read_release   = NAME##_read_release,\
            .write_acquire  = NAME##_write_acquire,\
            .write_request  = NAME##_write_request,\
            .write_release  = NAME##_write_release,\
            .destroy        = NAME##_destroy,\
    };\
    \
    NEW_STREAM_DECLARE(NAME)\
    {\
        camio_stream_t* result = (camio_stream_t*)calloc(1,sizeof(camio_stream_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        result->vtable = NAME##_stream_interface;\
        return result;\
    }




#endif /* STREAM_H_ */
