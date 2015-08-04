/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/device_params_vec.h>

#include "ttt_device.h"
#include "ttt_device.h"

static const char* const scheme = "ttt";


static camio_error_t construct(void** params, ch_word params_size, camio_dev_t** device_o)
{
    camio_dev_t* dev = NEW_DEVICE(ttt);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct(dev,params, params_size);
}




void ttt_init()
{

    DBG("Initializing ttt...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    //add_param_optional(params,"ra",ttt_params_t,rd_address,"");
    const ch_word hier_offset = offsetof(ttt_params_t,hierarchical);

    register_new_device(scheme,strlen(scheme),hier_offset,construct,sizeof(ttt_params_t),params,0);
    DBG("Initializing ttt...Done\n");
}
