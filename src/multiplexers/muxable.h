/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 17, 2014
 *  File name: muxable.h
 *  Description:
 *  Every channel and controller implements a "muxable" interface. This interface allows the channel or controller to be put
 *  into a "muxor object" for runtime nonblocking multiplexing of devices
 */

#ifndef MUXABLE_H_
#define MUXABLE_H_

#include <src/types/types.h>

typedef enum {
    CAMIO_MUX_MODE_NONE    = 0, //Default case. Error.
    CAMIO_MUX_MODE_READ    = 1, //Selector will fire when muxable is ready to read
    CAMIO_MUX_MODE_WRITE   = 2, //Selector will fire when muxable is ready to write
    CAMIO_MUX_MODE_CONNECT = 4, //Selector will fire when muxable is ready to connect
} camio_mux_mode_e;

/**
 * Every CamIO muxorable must implement this interface. See muxor.h and api.h for more details.
 */
typedef struct camio_muxable_interface_s{
    camio_error_t (*ready)(camio_muxable_t* this);
} camio_muxable_interface_t;


/**
 * This is a basic CamIO muxable structure. All muxables must implement this. Transports can use the macros provided
 * to overload this structure with and include a private data structures.
 */

typedef struct camio_muxable_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer, but saving a couple of bytes seems a little
     * silly when it will cost a pointer dereference on the critical path. YMMV.
     */
    camio_muxable_interface_t vtable;

    /**
     * A file descriptor for use in select/poll/epoll/kqueue etc. May be -1 if file descriptors are not supported by the
     * muxable type
     */
    int fd;

    union{
        camio_channel_t*    channel;    //Gives access back to the channel or controller which is the parent of this
        camio_controller_t* controller;
    } parent;

    /**
     * Let the mux know what sort of event's we are looking for.
     */
    camio_mux_mode_e mode;

    /**
     * Store an ID here for a simple way to know which channel this is. This is kind of a hack.
     */
    ch_word id;
} camio_muxable_t;



/**
 * Some macros to make life easier MUXABLE_GET_PRIVATE helps us grab the private members out of the public muxable_t
 */
#define MUXABLE_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_MUXABLE(name)\
        new_##name##_muxable()

#define NEW_MUXABLE_DECLARE(NAME)\
        camio_muxable_t* new_##NAME##_muxable()

#define NEW_MUXABLE_DEFINE(NAME, PRIVATE_TYPE, MODE) \
    const static camio_muxable_interface_t NAME##_muxable_interface = {\
            .ready = NAME##_ready,\
    };\
    \
    NEW_MUXABLE_DECLARE(NAME)\
    {\
        camio_muxable_t* result = (camio_muxable_t*)calloc(1,sizeof(camio_muxable_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        result->vtable = NAME##_muxable_interface;\
        result->mode   = MODE;\
        return result;\
    }






#endif /* MUXABLE_H_ */
