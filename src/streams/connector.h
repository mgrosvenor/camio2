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



struct cioconn_s {
    /**
     * Return the the selectable structure for adding into a selector
     */
    cioselable selectable;


    /**
     * Non-blocking attempt to connect the underlying stream to it’s data source. If successful, the CamIO stream can be read
     *  and/or written to. In many cases, the connect operation will return immediately, with a valid stream once only.
     *  However, this is not guaranteed. Some streams may return multiple valid connection and some streams may take some
     *  time before they return successfully. Streams can be placed into selectors to facilitate the blocking behaviour
     *  necessary to wait for these events to happen by listening for the  on connection signal.
     *  Return values;
     *  - ENOERROR: Stream was created successfully.
     *  - ETRYAGAIN: The stream has nothing new to report at this time. No connection has yet happened.
     *  - ECHECKERROR: The stream creation has failed.
     */
    int (*connect)( cioconn* this, ciostrm* ciostrm_o );

    /**
     * Free resources associated with this connector, but not with any of the streams it has created.
     */
    void (*destroy)(cioconn* this);


    /**
     * Pointer to stream specific data structures.
     */
    void* __priv;
};


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
camio_error_t new_cioconn( char* uri , ciostrm_req* properties, struct cioconn_s** cioconn_o );


#endif /* CONNECTOR_H_ */
