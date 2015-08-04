/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   22 Jun 2015
 *  File name: tcp_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/drivers/tcp/tcp_device.h>
#include <src/drivers/tcp/tcp_device.h>
#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/device_params_vec.h>

static const char* const scheme = "tcp";


static camio_error_t construct(void** params, ch_word params_size, camio_dev_t** device_o)
{
    camio_dev_t* dev = NEW_DEVICE(tcp);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct(dev,params, params_size);
}




void tcp_init()
{

    DBG("Initializing TCP...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    add_param_optional(params,"listen",tcp_params_t,listen, 0);
    add_param_optional(params,"rd_buff_sz",tcp_params_t,rd_buff_sz, (16 * 1024 * 1024));
    add_param_optional(params,"wr_buff_sz",tcp_params_t,wr_buff_sz, (16 * 1024 * 1024));
    //TODO XXX -- add the rest of the TCP options here...
    const ch_word hier_offset = offsetof(tcp_params_t,hierarchical);

    register_new_device(scheme,strlen(scheme),hier_offset,construct,sizeof(tcp_params_t),params,0);
    DBG("Initializing TCP...Done\n");
}
