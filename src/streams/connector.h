/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 25, 2014
 *  File name: connector.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef CONNECTOR_H_
#define CONNECTOR_H_

#include <stdlib.h>
#include <types/types.h>
#include "features.h"

/**
 * Every CamIO stream must implement this interface, see function prototypes in api.h
 */

typedef struct camio_connector_interface_s{
    int (*connect)( camio_connector_t* this, camio_stream_t** stream_o );
    void (*destroy)(camio_connector_t* this);
} camio_connector_interface_t;

/**
 * This is a basic CamIO connector structure. All streams must implement this. Streams will use the macros provided to
 * overload this structure with and include a private data structures..
 */

typedef struct camio_connector_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer, but saving 2x8bytes seems a little silly
     * when it will cost a pointer dereference on the critical
     * path.
     */
    camio_connector_interface_t vtable;

    /**
     * Return the the selectable structure for adding into a selector
     */
    cioselable selectable;
} camio_connector_t;



/**
 * Some macros to make life easier CONNECTOR_GET_PRIVATE helps us grab the private members out of the public connector_t and
 * CONNECTOR_DECLARE help to make a custom allocator for each stream
 */
#define CONNECTOR_GET_PRIVATE(this) ( (void*)(this + 1))

#define CONNECTOR_DECLARE(connector_private_type) \
    const static camio_connector_interface_t connector_interface = {\
            .connect = connect,\
            .destroy = connector_destroy,\
    };\
    \
    static camio_connector_t* new_connector()\
    {\
        camio_connector_t* result = (camio_connector_t*)calloc(sizeof(camio_connector_t) + sizeof(connector_private_type));\
        result->vtable = connector_interface;\
        return result;\
    }\



#endif /* CONNECTOR_H_ */
