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
#include "../types/types.h"
#include "features.h"
#include "../selectors/selectable.h"

/**
 * Every CamIO stream must implement this interface, see function prototypes in api.h
 */

typedef struct camio_connector_interface_s{
    camio_error_t (*construct)(camio_connector_t* this, void* parameters, ch_word parameters_size );
    camio_error_t (*connect)( camio_connector_t* this, camio_stream_t** stream_o );
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
     * Return the the selectable structure for adding into a selector. Not valid until the connector constructor has been
     * called.
     */
    camio_selectable_t selectable;

    /**
     * Return the features supported by this transport. Not valid until the connector constructor has been called.
     */
    camio_transport_features_t features;
} camio_connector_t;



/**
 * Some macros to make life easier CONNECTOR_GET_PRIVATE helps us grab the private members out of the public connector_t and
 * CONNECTOR_DECLARE help to make a custom allocator for each stream
 */
#define CONNECTOR_GET_PRIVATE(THIS) ( (void*)(THIS + 1))

#define NEW_CONNECTOR(name)\
        new_##name##_connector

#define CONNECTOR_DECLARE(NAME)\
        camio_connector_t* new_##NAME##_connector()

#define CONNECTOR_DEFINE(NAME, PRIVATE_TYPE) \
    const static camio_connector_interface_t NAME##_connector_interface = {\
            .construct = construct,\
            .connect   = connect,\
            .destroy   = destroy,\
    };\
    \
    CONNECTOR_DECLARE(NAME)\
    {\
        camio_connector_t* result = (camio_connector_t*)calloc(sizeof(camio_connector_t) + sizeof(PRIVATE_TYPE));\
        result->vtable = NAME##_connector_interface;\
        return result;\
    }\



#endif /* CONNECTOR_H_ */
