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

#include "netmap_connector.h"


typedef struct netmap_priv_s {
    int fd;
} netmap_priv_t;


static camio_error_t construct_str(camio_connector_t* this, camio_uri_t* uri)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)uri;
    return CAMIO_NOTIMPLEMENTED;
}

static camio_error_t construct_bin(camio_connector_t* this, va_list args)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)args;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)stream_o;
    return CAMIO_NOTIMPLEMENTED;
}


static void destroy(camio_connector_t* this)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
}


CONNECTOR_DEFINE(netmap, netmap_priv_t)

