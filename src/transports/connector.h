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

#include <src/transports/features.h>
#include <stdlib.h>
#include "../types/types.h"
#include "../selectors/selectable.h"
#include "../utils/uri_parser/uri_parser.h"



typedef camio_error_t (*camio_construct_str_f)( camio_uri_t* uri, camio_connector_t** connector_o);
typedef camio_error_t (*camio_construct_bin_f)( camio_connector_t** connector_o, va_list args);


typedef camio_error_t (*camio_conn_construct_str_f)( camio_connector_t* this, camio_uri_t* uri);
typedef camio_error_t (*camio_conn_construct_bin_f)( camio_connector_t* this, va_list args);


/**
 * Every CamIO stream must implement this interface, see function prototypes in api.h
 */

typedef struct camio_connector_interface_s{
    camio_conn_construct_str_f construct_str;
    camio_conn_construct_bin_f construct_bin;
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
            .construct_str = construct_str,\
            .construct_bin = construct_bin,\
            .connect   = cconnect,\
            .destroy   = destroy,\
    };\
    \
    CONNECTOR_DECLARE(NAME)\
    {\
        camio_connector_t* result = (camio_connector_t*)calloc(1,sizeof(camio_connector_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        result->vtable = NAME##_connector_interface;\
        return result;\
    }



#endif /* CONNECTOR_H_ */
