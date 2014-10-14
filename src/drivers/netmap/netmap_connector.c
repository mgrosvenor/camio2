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

#include <src/drivers/netmap/netmap_connector.h>
#include "../../camio.h"


typedef struct netmap_priv_s {
    int fd;
} netmap_priv_t;


static camio_error_t construct_str(camio_connector_t* this, camio_uri_t* uri)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)uri;

    void* global_store = NULL;
    camio_transport_get_global("netm", 4, &global_store);
    printf("Netmap construct str - global store at %p\n", global_store);

    return CAMIO_NOTIMPLEMENTED;
}

static camio_error_t construct_bin(camio_connector_t* this, va_list args)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)args;

    void* global_store = NULL;
    camio_transport_get_global("netm", 4, &global_store);
    printf("Netmap construct bin - global store at %p\n", global_store);


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

