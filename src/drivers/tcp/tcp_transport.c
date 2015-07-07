/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   22 Jun 2015
 *  File name: tcp_transport.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/drivers/tcp/tcp_connector.h>
#include <src/drivers/tcp/tcp_transport.h>
#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/transport_params_vec.h>

static const char* const scheme = "tcp";


static camio_error_t construct(void** params, ch_word params_size, camio_connector_t** connector_o)
{
    camio_connector_t* conn = NEW_CONNECTOR(tcp);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}




void tcp_init()
{

    DBG("Initializing TCP...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this transport
    }

    add_param_optional(params,"listen",tcp_params_t,listen, 0);
    add_param_optional(params,"rd_buff_sz",tcp_params_t,rd_buff_sz, (16 * 1024 * 1024));
    add_param_optional(params,"wr_buff_sz",tcp_params_t,wr_buff_sz, (16 * 1024 * 1024));
    //TODO XXX -- add the rest of the TCP options here...
    const ch_word hier_offset = offsetof(tcp_params_t,hierarchical);

    register_new_transport(scheme,strlen(scheme),hier_offset,construct,sizeof(tcp_params_t),params,0);
    DBG("Initializing TCP...Done\n");
}
