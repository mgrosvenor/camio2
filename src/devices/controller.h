/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 25, 2014
 *  File name: controller.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <src/devices/features.h>
#include <stdlib.h>
#include <src/types/types.h>
#include <src/multiplexers/muxable.h>
#include <src/utils/uri_parser/uri_parser.h>

#include "messages.h"

#define CAMIO_CTRL_REQ_INF 0x001


typedef camio_error_t (*camio_conn_construct_f)(camio_controller_t* controller_o, void** params, ch_word params_size);


/**
 * Every CamIO channel must implement this interface, see function prototypes in api.h for more details
 */
typedef struct camio_controller_interface_s{
    camio_conn_construct_f construct;
    camio_error_t (*channel_request)( camio_controller_t* this, camio_msg_t* req_vec, ch_word* vec_len_io);
    camio_error_t (*channel_result)( camio_controller_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);
    void (*destroy)(camio_controller_t* this);
} camio_controller_interface_t;



/**
 * This is a basic CamIO controller structure. All channels must implement this. Streams will use the macros provided to
 * overload this structure with and include a private data structures..
 */

typedef struct camio_controller_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer, but saving a couple of bytes seems a little
     * silly when it will cost a pointer dereference on the critical path. YMMV.
     */
    camio_controller_interface_t vtable;

    /**
     * Return the the selectable structure for adding into a selector. Not valid until the controller constructor has been
     * called.
     */
    camio_muxable_t muxable;

    /**
     * Return the features supported by this device. Not valid until the controller constructor has been called.
     */
    camio_device_features_t features;
} camio_controller_t;



/**
 * Some macros to make life easier CONTROLLER_GET_PRIVATE helps us grab the private members out of the public controller_t and
 * CONTROLLER_DECLARE help to make a custom allocator for each channel
 */
#define CONTROLLER_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_CONTROLLER(name)\
        new_##name##_controller()

#define NEW_CONTROLLER_DECLARE(NAME)\
        camio_controller_t* new_##NAME##_controller()

#define NEW_CONTROLLER_DEFINE(NAME, PRIVATE_TYPE) \
    static const camio_controller_interface_t NAME##_controller_interface = {\
            .construct         = NAME##_construct,\
            .channel_request   = NAME##_channel_request,\
            .channel_result    = NAME##_channel_result,\
            .destroy           = NAME##_destroy,\
    };\
    \
    NEW_CONTROLLER_DECLARE(NAME)\
    {\
        camio_controller_t* result = (camio_controller_t*)calloc(1,sizeof(camio_controller_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        if(!result) return ((void *)0);\
        result->vtable                      = NAME##_controller_interface;\
        result->muxable.mode                = CAMIO_MUX_MODE_CONNECT;\
        result->muxable.parent.controller   = result;\
        result->muxable.vtable.ready        = NAME##_channel_ready;\
        result->muxable.fd                  = -1;\
        return result;\
    }



#endif /* CONTROLLER_H_ */
