/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stddef.h>

#include <src/camio.h>
#include <deps/chaste/utils/debug.h>
#include <src/types/device_params_vec.h>

#include "delim_device.h"
#include "delim_device.h"

static const char* const scheme = "delim";


static camio_error_t construct(void** params, ch_word params_size, camio_device_t** device_o)
{
    camio_device_t* dev = NEW_DEVICE(delim);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct(dev,params, params_size);
}




void delim_init()
{

    DBG("Initializing delim...\n");

    //No params to add. The delimiter should only be used in binary form!
    register_new_device(scheme,strlen(scheme),0,construct,0,NULL,0);
    DBG("Initializing delim...Done\n");
}
