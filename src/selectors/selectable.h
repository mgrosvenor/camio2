/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 17, 2014
 *  File name: selectable.h
 *  Description:
 *  Every stream and connector implements a "selectable" interface. This interface allows the stream or connector to be put
 *  into a "selector object" for runtime nonblocking multiplexing of transports
 */

#ifndef SELECTABLE_H_
#define SELECTABLE_H_

#include <src/types/types.h>

/**
 * Every CamIO selectorable must implement this interface. See selector.h and api.h for more details.
 */
typedef struct camio_selectable_interface_s{
    camio_error_t (*ready)(camio_selectable_t* this);
} camio_selectable_interface_t;


/**
 * This is a basic CamIO selectable structure. All selectables must implement this. Transports will use the macros provided
 * to overload this structure with and include a private data structures.
 */

typedef struct camio_selectable_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer, but saving a couple of bytes seems a little
     * silly when it will cost a pointer dereference on the critical path. YMMV.
     */
    camio_selectable_interface_t vtable;

    /**
     * A file descriptor for use in select/poll/epoll/kqueue etc. May be -1 if file descriptors are not supported by the
     * selectable type
     */
    int fd;

    camio_stream_t*    parent_stream;    //For selectables that derive from a stream, this gives access back to the stream
    camio_connector_t* parent_connector; //For selectables that derive from a connector, this give access back to the
                                         //connector
} camio_selectable_t;



/**
 * Some macros to make life easier SELECTABLE_GET_PRIVATE helps us grab the private members out of the public selectable_t
 */
#define SELECTABLE_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_SELECTABLE(name)\
        new_##name##_selectable()

#define NEW_SELECTABLE_DECLARE(NAME)\
        camio_selectable_t* new_##NAME##_selectable()

#define NEW_SELECTABLE_DEFINE(NAME, PRIVATE_TYPE) \
    const static camio_selectable_interface_t NAME##_selectable_interface = {\
            .ready = NAME##_ready,\
    };\
    \
    NEW_SELECTABLE_DECLARE(NAME)\
    {\
        camio_selectable_t* result = (camio_selectable_t*)calloc(1,sizeof(camio_selectable_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        result->vtable = NAME##_selectable_interface;\
        return result;\
    }






#endif /* SELECTABLE_H_ */
