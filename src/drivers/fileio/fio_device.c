/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/device_params_vec.h>

#include "fio_device.h"
#include "fio_device.h"

static const char* const scheme = "fio";


static camio_error_t construct(void** params, ch_word params_size, camio_device_t** device_o)
{
    camio_device_t* dev = NEW_DEVICE(fio);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct(dev,params, params_size);
}




void fio_init()
{

    DBG("Initializing fio...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    //add_param_optional(params,"ra",fio_params_t,rd_address,"");
    add_param_optional(params,"rd_buff_sz",fio_params_t,rd_buff_sz, 16 * 1024);
    add_param_optional(params,"wr_buff_sz",fio_params_t,wr_buff_sz, 16 * 1024);
    add_param_optional(params,"rd_fd",fio_params_t,rd_fd, -1);
    add_param_optional(params,"wr_fd",fio_params_t,wr_fd, -1);
    add_param_optional(params,"RO",fio_params_t,rd_only, 0); //read only
    add_param_optional(params,"WO",fio_params_t,wr_only, 0); //write only
    const ch_word hier_offset = offsetof(fio_params_t,hierarchical);

    register_new_device(scheme,strlen(scheme),hier_offset,construct,sizeof(fio_params_t),params,0);
    DBG("Initializing fio...Done\n");
}
