/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: stdio_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/device_params_vec.h>

#include "stdio_device.h"
#include "stdio_device.h"

static const char* const scheme = "stdio";


static camio_error_t construct(void** params, ch_word params_size, camio_device_t** device_o)
{
    camio_device_t* dev = NEW_DEVICE(stdio);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct(dev,params, params_size);
}


void stdio_init()
{
    DBG("*** Initializing stdio...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    add_param_optional(params,"rd_buff_sz",stdio_params_t,rd_buff_sz, 16 * 1024);
    add_param_optional(params,"wr_buff_sz",stdio_params_t,wr_buff_sz, 16 * 1024);
    add_param_optional(params,"RO",stdio_params_t,rd_only, 0); //read only
    add_param_optional(params,"WO",stdio_params_t,wr_only, 0); //write only
    const ch_word hier_offset = offsetof(stdio_params_t,__hierarchical__);

    register_new_device(scheme,strlen(scheme),hier_offset,construct,sizeof(stdio_params_t),params,0);
    DBG("Initializing stdio...Done\n");
}
