/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 17, 2014
 *  File name: muxable.h
 *  Description:
 *  Every channel and device implements a "muxable" interface. This interface allows the channel or device to be put
 *  into a "muxor object" for runtime nonblocking multiplexing of devices
 */

#ifndef MUXABLE_H_
#define MUXABLE_H_

#include <src/types/types.h>

typedef enum {
    CAMIO_MUX_MODE_NONE,         //Default case. Error.
    CAMIO_MUX_MODE_READ_DATA,    ///Selector will fire when muxable is ready to read
    CAMIO_MUX_MODE_READ_BUFF,    //Selector will fire when muxable is ready to give a new buffer
    CAMIO_MUX_MODE_WRITE_BUFF,   //Selector will fire when muxable is ready to give a new buffer
    CAMIO_MUX_MODE_WRITE_DATA,   //Selector will fire when muxable is ready to write
    CAMIO_MUX_MODE_CONNECT,      //Selector will fire when muxable is ready to devect
} camio_mux_mode_e;

/**
 * Every CamIO muxorable must implement this interface. See muxor.h and api.h for more details.
 */
typedef struct camio_muxable_interface_s{
    camio_error_t (*ready)(camio_muxable_t* this);
} camio_muxable_interface_t;

typedef camio_error_t (*mux_callback_f)(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id);

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

    /**
     * Let the mux know what sort of event's we are looking for.
     */
    camio_mux_mode_e mode;

    /**
     * Gives access back to the channel or device (figure out which by looking at the mux_mode above
     */
    union{
        camio_channel_t*    channel;
        camio_device_t* device;
    } parent;

    //Let users keep a pointer to some local state, this is ignored by CamIO
    void* usr_state;

    //Call back for users when this muxable is ready
    mux_callback_f callback;

    //To easily identify this muxable
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
