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


/**
 * Every CamIO stream must implement this interface, see function prototypes in api.h
 */
typedef struct camio_stream_interface_s{

    void (*destroy)(camio_stream_t* this);

    //Read operations
    camio_error_t (*read_request)(camio_stream_t* this, ch_word buffer_offset, ch_word source_offset, ch_word size_hint);
    camio_error_t (*read_acquire)(camio_stream_t* this, camio_rd_buffer_t** buffer_o);
    camio_error_t (*read_release)(camio_stream_t* this, camio_wr_buffer_t** buffer_o);

    //Write operations
    camio_error_t (*write_acquire)(camio_stream_t* this, camio_wr_buffer_t** buffer_o);
    camio_error_t (*write_commit)( camio_stream_t* this, camio_wr_buffer_t** buffer_chain );
    camio_error_t (*write_release)(camio_stream_t* this, camio_wr_buffer_t** buffers_chain);

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
            .write_commit   = NAME##_write_commit,\
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
