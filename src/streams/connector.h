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



#endif /* CONNECTOR_H_ */
