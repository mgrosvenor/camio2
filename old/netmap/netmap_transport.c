/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Oct 13, 2014
 *  File name: netmap_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <src/drivers/netmap/netmap_device.h>
#include <src/drivers/netmap/netmap_device.h>
#include "../../camio.h"
#include "../../camio_debug.h"

const char * const scheme = "netm";

static camio_error_t construct_str(camio_uri_t* uri, camio_device_t** device_o)
{
    camio_device_t* dev = NEW_DEVICE(netmap);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;
    DBG("Connector address=%p (%p)\n",dev, *device_o);

    return dev->vtable.construct_str(dev,uri);
}

static camio_error_t construct_bin(camio_device_t** device_o, va_list args)
{
    camio_device_t* dev = NEW_DEVICE(netmap);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct_bin(dev,args);
}


void netmap_init()
{
    register_new_device(scheme,strlen(scheme),construct_str, construct_bin,0);
}
