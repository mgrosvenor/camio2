/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 14, 2015
 *  File name:  selector.h
 *  Description:
 *  Every stream and connector implements a "selector" interface. This interface allows the stream or connector to be put
 *  into a "selector object" for runtime nonblocking multiplexing of transports
 */
#ifndef SELECTOR_H_
#define SELECTOR_H_

#include <src/types/types.h>
typedef enum {
    CAMIO_SELECTOR_MODE_NONE    = 0, //Just in case this is useful
    CAMIO_SELECTOR_MODE_READ    = 1, //Selector will fire when selectable is ready to read
    CAMIO_SELECTOR_MODE_WRITE   = 2, //Selector will fire when selectable is ready to write
    CAMIO_SELECTOR_MODE_CONNECT = 4, //Selector will fire when selectable is ready to connect
} camio_selector_mode_e;


/**
 * Every CamIO selector must implement this interface. See selector.h and api.h for more details.
 */
typedef struct camio_selector_interface_s{
    camio_error_t (*insert)(camio_selector_t* this, camio_selectable_t* selectable, camio_selector_mode_e modes);
    camio_error_t (*remove)(camio_selector_t* this, camio_selectable_t* selectable);
    camio_error_t (*select)(camio_selector_t* this, struct timespec timeout, camio_selectable_t** selectable_o,
            camio_selector_mode_e* modes_o);
    void (*destroy)(camio_selector_t* this);
} camio_selector_interface_t;


/**
 * This is a basic CamIO selector structure. All selectors must implement this. Transports will use the macros provided
 * to overload this structure with and include a private data structures.
 */

typedef struct camio_selector_s {
    /**
     * vtable that holds the function pointers, usually this would be a pointer, but saving a couple of bytes seems a little
     * silly when it will cost a pointer dereference on the critical path. YMMV.
     */
    camio_selector_interface_t vtable;

} camio_selector_t;



/**
 * Some macros to make life easier SELECTOR_GET_PRIVATE helps us grab the private members out of the public selector_t
 */
#define SELECTOR_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_SELECTOR(name)\
        new_##name##_selector()

#define NEW_SELECTOR_DECLARE(NAME)\
        camio_selector_t* new_##NAME##_selector()

#define NEW_SELECTOR_DEFINE(NAME, PRIVATE_TYPE) \
    const static camio_selector_interface_t NAME##_selector_interface = {\
            .ready = NAME##_ready,\
    };\
    \
    NEW_SELECTOR_DECLARE(NAME)\
    {\
        camio_selector_t* result = (camio_selector_t*)calloc(1,sizeof(camio_selector_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        result->vtable = NAME##_selector_interface;\
        return result;\
    }


#endif /* SELECTOR_H_ */
