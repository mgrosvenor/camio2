/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: netmap_connector.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "connector.h"


typedef struct netmap_priv_s {
    int fd;
} netmap_priv_t;


static camio_error_t construct(void* parameters, ch_word parameters_size )
{

}


static camio_error_t connect(camio_connector_t* this, camio_stream_t* stream_o )
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    return CAMIO_NOTIMPLEMENTED;
}


static void connector_destroy(camio_connector_t* this)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
}


CONNECTOR_DECLARE(netmap_priv_t);


camio_error_t new_netmap_connector_bin_va( camio_connector_t** connector_o, camio_stream_features_t* properties,
        va_list args )
{
    camio_connector_t* this = new_connector();
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    *connector_o = this;


    return CAMIO_NOTIMPLEMENTED;
}


camio_error_t new_netmap_connector_uri( camio_connector_t** connector_o, char* uri , camio_stream_features_t* properties )
{
    return CAMIO_NOTIMPLEMENTED;
}

