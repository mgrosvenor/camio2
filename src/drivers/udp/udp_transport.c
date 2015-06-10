/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_transport.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/drivers/udp/udp_connector.h>
#include <src/drivers/udp/udp_transport.h>
#include "../../camio.h"
#include "../../camio_debug.h"
#include <src/types/transport_params_vec.h>

static const char* const scheme = "udp";


static camio_error_t construct(void** params, ch_word params_size, camio_connector_t** connector_o)
{
    camio_connector_t* conn = NEW_CONNECTOR(udp);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}




void udp_init()
{

    DBG("Initializing UDP...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,NULL);
    if(!params){
        return; // No memory. Can't register this transport
    }


    add_param_required(params,"wr_protocol",udp_params_t,wr_protocol);
    add_param_required(params,"rd_protocol",udp_params_t,rd_protocol);
    add_param_required(params,"rd_address",udp_params_t,rd_address);
    add_param_required(params,"wr_address",udp_params_t,wr_address);

    char* temp; //TODO XXX BUG THIS IS BROKEN. PLACE HOLDER TO GET THINGS TO COMPILE

    register_new_transport(scheme,strlen(scheme),&temp,construct,sizeof(udp_params_t),params,0);
    DBG("Initializing UDP...Done\n");
}
