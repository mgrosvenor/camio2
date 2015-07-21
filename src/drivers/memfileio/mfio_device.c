/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   01 Jul 2015
 *  File name: mfio_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/device_params_vec.h>

#include "mfio_controller.h"
#include "mfio_device.h"

static const char* const scheme = "mfio";


static camio_error_t construct(void** params, ch_word params_size, camio_controller_t** controller_o)
{
    camio_controller_t* conn = NEW_CONNECTOR(mfio);
    if(!conn){
        *controller_o = NULL;
        return CAMIO_ENOMEM;
    }

    *controller_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}




void mfio_init()
{

    DBG("Initializing mfio...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    //add_param_optional(params,"ra",mfio_params_t,rd_address,"");
    const ch_word hier_offset = offsetof(mfio_params_t,hierarchical);

    register_new_device(scheme,strlen(scheme),hier_offset,construct,sizeof(mfio_params_t),params,0);
    DBG("Initializing mfio...Done\n");
}
