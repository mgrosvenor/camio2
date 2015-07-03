/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_transport.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/transport_params_vec.h>

#include "fio_connector.h"
#include "fio_transport.h"

static const char* const scheme = "fio";


static camio_error_t construct(void** params, ch_word params_size, camio_connector_t** connector_o)
{
    camio_connector_t* conn = NEW_CONNECTOR(fio);
    if(!conn){
        *connector_o = NULL;
        return CAMIO_ENOMEM;
    }

    *connector_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}




void fio_init()
{

    DBG("Initializing fio...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,NULL);
    if(!params){
        return; // No memory. Can't register this transport
    }

    //add_param_optional(params,"ra",fio_params_t,rd_address,"");
    add_param_optional(params,"rd_buff_sz",fio_params_t,rd_buff_sz, 32 * 1024);
    add_param_optional(params,"wr_buff_sz",fio_params_t,wr_buff_sz, 32 * 1024);
    add_param_optional(params,"rd_fd",fio_params_t,rd_fd, -1);
    add_param_optional(params,"wr_fd",fio_params_t,wr_fd, -1);
    add_param_optional(params,"RO",fio_params_t,rd_only, 0); //read only
    add_param_optional(params,"WO",fio_params_t,wr_only, 0); //write only
    const ch_word hier_offset = offsetof(fio_params_t,hierarchical);
    DBG("Hierarchical offset=%i...Done\n", hier_offset);

    register_new_transport(scheme,strlen(scheme),hier_offset,construct,sizeof(fio_params_t),params,0);
    DBG("Initializing fio...Done\n");
}
