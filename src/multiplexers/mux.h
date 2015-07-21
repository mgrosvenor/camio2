/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 14, 2015
 *  File name:  mux.h
 *  Description:
 *  Every channel and controller implements a "mux" interface. This interface allows the channel or controller to be put
 *  into a "mux object" for runtime nonblocking multiplexing of devices
 */
#ifndef MUX_H_
#define MUX_H_

#include <src/types/types.h>
#include "muxable.h"
#include <time.h>

/**
 * Every CamIO mux must implement this interface. See mux.h and api.h for more details.
 */
typedef struct camio_mux_interface_s{
    camio_error_t (*construct)(camio_mux_t* this);
    camio_error_t (*insert)(camio_mux_t* this, camio_muxable_t* muxable, ch_word id);
    camio_error_t (*remove)(camio_mux_t* this, camio_muxable_t* muxable);
    camio_error_t (*select)(camio_mux_t* this, /*struct timespec timeout,*/ camio_muxable_t** muxable_o, ch_word* which_o);
    ch_word (*count)(camio_mux_t* this);
    void (*destroy)(camio_mux_t* this);
} camio_mux_interface_t;


/**
 * This is a basic CamIO mux structure. All muxs must implement this. Transports will use the macros provided
 * to overload this structure with and include a private data structures.
 */

typedef struct camio_mux_s {
    /**
     * vtable that holds the function pointers, usually this would be a pointer, but saving a couple of bytes seems a little
     * silly when it will cost a pointer dereference on the critical path. YMMV.
     */
    camio_mux_interface_t vtable;

} camio_mux_t;



/**
 * Some macros to make life easier MUX_GET_PRIVATE helps us grab the private members out of the public mux_t
 */
#define MUX_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_MUX(name)\
        new_##name##_mux()

#define NEW_MUX_DECLARE(NAME)\
        camio_mux_t* new_##NAME##_mux()

#define NEW_MUX_DEFINE(NAME, PRIVATE_TYPE) \
    static const camio_mux_interface_t NAME##_mux_interface = {\
            .construct  = NAME##_construct,\
            .insert     = NAME##_insert,\
            .remove     = NAME##_remove,\
            .select     = NAME##_select,\
            .destroy    = NAME##_destroy,\
            .count      = NAME##_count,\
    };\
    \
    NEW_MUX_DECLARE(NAME)\
    {\
        camio_mux_t* result = (camio_mux_t*)calloc(1,sizeof(camio_mux_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        result->vtable = NAME##_mux_interface;\
        return result;\
    }


#endif /* MUX_H_ */
