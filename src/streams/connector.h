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

#include "../types/types.h"
#include "features.h"
#include "stream.h"

typedef struct camio_connector_interface_s{
    int (*connect)( camio_connector_t* this, camio_stream_t** stream_o );
    void (*destroy)(camio_connector_t* this);
} camio_connector_interface_t;


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
 *  Attempt to connect the underlying stream to it’s data source. This may or may not block. If successful, the CamIO stream
 *  can be read and/or written to. In many cases, the connect operation will return immediately, with a valid stream once
 *  only. However, this is not guaranteed. Some streams may return multiple valid connection and some streams may take some
 *  time before they return successfully. Streams can be placed into selectors to facilitate the blocking behaviour
 *  necessary to wait for these events to happen by listening for the  on connection signal.
 *  Return values;
 *  - ENOERROR: Stream was created successfully.
 *  - ETRYAGAIN: The stream has nothing new to report at this time. No connection has yet happened.
 *  - ECHECKERROR: The stream creation has failed.
 */
camio_error_t camio_connect( camio_connector_t* this, camio_stream_t* stream_o );

/**
 * Free resources associated with this connector, but not with any of the streams it has created.
 */
void camio_destroy(camio_connector_t* this);


/**
 * Create a new CamIO connector of type "type" with the properties requested and otherwise binary arguments. The type and
 * order of these arguments is stream specific. The camio_connector_o is only valid id ENOERROR is returned.
 * Returns:
 * - ENOERROR:  All good. Nothing to see here.
 * - ENOSTREAM: The stream type is unrecognized
 * - EBADOPT:   The arguments have an error or are unsupported
 * - EBADPROP:  The stream supplied in did not match the properties requested.
 */
camio_error_t camio_connector_new_bin( camio_connector_t** connector, camio_stream_type_t type,
                                       camio_stream_features_t* properties,  ... );


/**
 * Create a new CamIO connector with with given the URI and properties request. Return a pointer to connection object in
 * cioconn_o. Cioconn_o is only valid id ENOERROR is returned.
 * Returns:
 * - ENOERROR:  All good. Nothing to see here.
 * - EBADURI:   The URI supplied has a syntax error or is poorly formatted
 * - ENOSTREAM: The stream type is unrecognized
 * - EBADOPT:   The URI options have an error or are unsupported
 * - EBADPROP:  The stream supplied in URI did not match the properties requested.
 */
camio_error_t camio_connector_new_uri( camio_connector_t** connector, char* uri , camio_stream_features_t* properties );


#endif /* CONNECTOR_H_ */
