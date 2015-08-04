/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/drivers/udp/udp_device.h>
#include <src/drivers/udp/udp_device.h>
#include "../../camio.h"
#include "../../camio_debug.h"
#include <src/types/device_params_vec.h>

static const char* const scheme = "udp";


static camio_error_t construct(void** params, ch_word params_size, camio_dev_t** device_o)
{
    camio_dev_t* dev = NEW_DEVICE(udp);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct(dev,params, params_size);
}




void udp_init()
{

    DBG("Initializing UDP...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    add_param_optional(params,"ra",udp_params_t,rd_address,"");
    add_param_optional(params,"rp",udp_params_t,rd_protocol, "");
    add_param_optional(params,"wa",udp_params_t,wr_address, "");
    add_param_optional(params,"wp",udp_params_t,wr_protocol, "");
    add_param_optional(params,"rd_buff_sz",udp_params_t,rd_buff_sz, 64 * 1024);
    add_param_optional(params,"wr_buff_sz",udp_params_t,wr_buff_sz, 64 * 1024);
    //TODO XXX -- add readonly and write only flags!
    //TODO XXX -- add the rest of the UDP options here...
    const ch_word hier_offset = offsetof(udp_params_t,hierarchical);

    register_new_device(scheme,strlen(scheme),hier_offset,construct,sizeof(udp_params_t),params,0);
    DBG("Initializing UDP...Done\n");
}
