/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: stdio_transport.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/transport_params_vec.h>

#include "stdio_connector.h"
#include "stdio_transport.h"

static const char* const scheme = "stdio";


static camio_error_t construct(void** params, ch_word params_size, camio_connector_t** connector_o)
{
    camio_connector_t* conn = NEW_CONNECTOR(stdio);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}


void stdio_init()
{
    DBG("*** Initializing stdio...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,NULL);
    if(!params){
        return; // No memory. Can't register this transport
    }

    add_param_optional(params,"rd_buff_sz",stdio_params_t,rd_buff_sz, 32 * 1024);
    add_param_optional(params,"wr_buff_sz",stdio_params_t,wr_buff_sz, 32 * 1024);
    add_param_optional(params,"RO",stdio_params_t,rd_only, 0); //read only
    add_param_optional(params,"WO",stdio_params_t,wr_only, 0); //write only
    const ch_word hier_offset = offsetof(stdio_params_t,__hierarchical__);
    DBG("Hierarchical offset=%i...Done\n", hier_offset);

    register_new_transport(scheme,strlen(scheme),hier_offset,construct,sizeof(stdio_params_t),params,0);
    DBG("Initializing stdio...Done\n");
}
