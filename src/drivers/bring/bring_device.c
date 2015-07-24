/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <src/camio_debug.h>
#include <src/types/device_params_vec.h>

#include "bring_channel.h"
#include "bring_controller.h"
#include "bring_device.h"


static const char* const scheme = "bring";

#define CAMIO_BRING_SLOT_COUNT_DEFAULT 1024
#define CAMIO_BRING_SLOT_SIZE_DEFAULT (1024-sizeof(bring_slot_header_t))

static camio_error_t construct(void** params, ch_word params_size, camio_controller_t** controller_o)
{
    camio_controller_t* conn = NEW_CONTROLLER(bring);
    if(!conn){
        *controller_o = NULL;
        return CAMIO_ENOMEM;
    }

    *controller_o = conn;

    return conn->vtable.construct(conn,params, params_size);
}




void bring_init()
{

    DBG("Initializing bring...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    add_param_optional(params,"slots",bring_params_t,slots, CAMIO_BRING_SLOT_COUNT_DEFAULT);
    add_param_optional(params,"slot_sz",bring_params_t,slot_sz,CAMIO_BRING_SLOT_SIZE_DEFAULT);
    add_param_optional(params,"server",bring_params_t,server, 0);
    add_param_optional(params,"expand",bring_params_t,expand, 1);
    const ch_word hier_offset = offsetof(bring_params_t,hierarchical);

    register_new_device(scheme,strlen(scheme),hier_offset,construct,sizeof(bring_params_t),params,0);
    DBG("Initializing bring...Done\n");
}
