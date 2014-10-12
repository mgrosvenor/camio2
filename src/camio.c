/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: camio.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stdio.h>

#include "camio.h"
#include "camio_init_all.h"

#include "transports/netmap/netmap_transport.h"

camio_t* init_camio()
{
    printf("Initializing CamIO 2.0...\n");
    return &__camio_state_container;
}


camio_error_t new_camio_transport( camio_connector_t** connector_o, camio_transport_type_t type,
        camio_transport_features_t* features,  void* parameters, ch_word parameters_size)
{

    (void)connector_o;
    (void)features;
    (void)parameters;
    (void)parameters_size;


    if(!__camio_state_container.is_initialized){
        init_camio();
    }

    switch(type){
        case CAMIO_TRANSPORT_NETMAP:
    }



    return CAMIO_NOTIMPLEMENTED;
}
