/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 25, 2014
 *  File name: device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef DEVICE_H_
#define DEVICE_H_

#include <src/devices/features.h>
#include <stdlib.h>
#include <src/types/types.h>
#include <src/multiplexers/muxable.h>
#include <src/utils/uri_parser/uri_parser.h>

#include "messages.h"

#define CAMIO_CTRL_REQ_INF 0x001


typedef camio_error_t (*camio_dev_construct_f)(camio_device_t* device_o, void** params, ch_word params_size);


/**
 * Every CamIO channel must implement this interface, see function prototypes in api.h for more details
 */
typedef struct camio_device_interface_s{
    camio_dev_construct_f construct;
    camio_error_t (*channel_request)( camio_device_t* this, camio_msg_t* req_vec, ch_word* vec_len_io);
    camio_error_t (*channel_result)( camio_device_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);
    void (*destroy)(camio_device_t* this);
} camio_device_interface_t;



/**
 * This is a basic CamIO device structure. All channels must implement this. Streams will use the macros provided to
 * overload this structure with and include a private data structures..
 */

typedef struct camio_device_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer, but saving a couple of bytes seems a little
     * silly when it will cost a pointer dereference on the critical path. YMMV.
     */
    camio_device_interface_t vtable;

    /**
     * Return the the selectable structure for adding into a selector. Not valid until the device constructor has been
     * called.
     */
    camio_muxable_t muxable;

    /**
     * Return the features supported by this device. Not valid until the device constructor has been called.
     */
    camio_device_features_t features;
} camio_device_t;



/**
 * Some macros to make life easier DEVICE_GET_PRIVATE helps us grab the private members out of the public device_t and
 * DEVICE_DECLARE help to make a custom allocator for each channel
 */
#define DEVICE_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_DEVICE(name)\
        new_##name##_device

#define NEW_DEVICE_DECLARE(NAME)\
        camio_error_t new_##NAME##_device(void** params, ch_word params_size, camio_device_t** device_o)

#define NEW_DEVICE_DEFINE(NAME, PRIVATE_TYPE) \
    static const char* const scheme = #NAME;\
    \
    static const camio_device_interface_t NAME##_device_interface = {\
            .construct         = NAME##_construct,\
            .channel_request   = NAME##_channel_request,\
            .channel_result    = NAME##_channel_result,\
            .destroy           = NAME##_destroy,\
    };\
    \
    NEW_DEVICE_DECLARE(NAME)\
    {\
        camio_device_t* result = (camio_device_t*)calloc(1,sizeof(camio_device_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return CAMIO_ENOMEM;\
        *device_o = result; \
        result->vtable                      = NAME##_device_interface;\
        result->muxable.mode                = CAMIO_MUX_MODE_CONNECT;\
        result->muxable.parent.device       = result;\
        result->muxable.vtable.ready        = NAME##_channel_ready;\
        result->muxable.fd                  = -1;\
        \
        return result->vtable.construct(result,params, params_size);\
    }



#endif /* DEVICE_H_ */
