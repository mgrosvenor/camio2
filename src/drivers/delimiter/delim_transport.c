/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_transport.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/transport_params_vec.h>

#include "delim_connector.h"
#include "delim_transport.h"

static const char* const scheme = "delim";


static camio_error_t construct(void** params, ch_word params_size, camio_connector_t** connector_o)
{
    camio_connector_t* conn = NEW_CONNECTOR(delim);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}




void delim_init()
{

    DBG("Initializing delim...\n");

    //No params to add. The delimiter should only be used in binary form!
    register_new_transport(scheme,strlen(scheme),0,construct,0,NULL,0);
    DBG("Initializing delim...Done\n");
}
