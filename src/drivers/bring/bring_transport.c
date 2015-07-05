/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_transport.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/transport_params_vec.h>

#include "bring_stream.h"
#include "bring_connector.h"
#include "bring_transport.h"


static const char* const scheme = "bring";

#define CAMIO_BRING_SLOT_COUNT_DEFAULT 1024
#define CAMIO_BRING_SLOT_SIZE_DEFAULT (1024-sizeof(bring_slot_header_t))

static camio_error_t construct(void** params, ch_word params_size, camio_connector_t** connector_o)
{
    camio_connector_t* conn = NEW_CONNECTOR(bring);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}




void bring_init()
{

    DBG("Initializing bring...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this transport
    }

    add_param_optional(params,"slots",bring_params_t,slots, CAMIO_BRING_SLOT_COUNT_DEFAULT);
    add_param_optional(params,"slot_sz",bring_params_t,slot_sz,CAMIO_BRING_SLOT_SIZE_DEFAULT);
    add_param_optional(params,"block",bring_params_t,blocking,1);  //Ring will stop if it is full
    add_param_optional(params,"server",bring_params_t,server, 0);
    add_param_optional(params,"expand",bring_params_t,expand, 1);
    const ch_word hier_offset = offsetof(bring_params_t,hierarchical);

    register_new_transport(scheme,strlen(scheme),hier_offset,construct,sizeof(bring_params_t),params,0);
    DBG("Initializing bring...Done\n");
}
