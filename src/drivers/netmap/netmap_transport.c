/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Oct 13, 2014
 *  File name: netmap_transport.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <src/drivers/netmap/netmap_connector.h>
#include <src/drivers/netmap/netmap_transport.h>
#include "../../camio.h"
#include "../../camio_debug.h"

const char * const scheme = "netm";

static camio_error_t construct_str(camio_uri_t* uri, camio_connector_t** connector_o)
{
    camio_connector_t* conn = NEW_CONNECTOR(netmap);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;
    DBG("Connector address=%p (%p)\n",conn, *connector_o);

    return conn->vtable.construct_str(conn,uri);
}

static camio_error_t construct_bin(camio_connector_t** connector_o, va_list args)
{
    camio_connector_t* conn = NEW_CONNECTOR(netmap);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;

    return conn->vtable.construct_bin(conn,args);
}


void netmap_init()
{
    register_new_transport(scheme,strlen(scheme),construct_str, construct_bin,0);
}