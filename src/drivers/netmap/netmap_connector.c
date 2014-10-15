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

#include "netmap_transport.h"
#include "netmap_connector.h"
#include "../../camio.h"
#include "../../camio_debug.h"


typedef struct netmap_priv_s {
    int fd;
} netmap_priv_t;


static camio_error_t construct(camio_connector_t* this, ch_cstr dev, ch_cstr path, ch_bool use_host_ring, ch_word ring_id)
{

    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    netmap_global_t* global = NULL;
    camio_error_t result = camio_transport_get_global("netm", (void**)&global);
    if(result){
        return result;
    }


    DBG("Netmap  global store at \n");

    //Already initialised, so we can just reuse that state
    if(global->is_init){
        DBG("Reusing global state\n");
        priv->fd = global->fd;
        return CAMIO_ENOERROR;
    }

    DBG("No global state\n");


    global->is_init = true;
    DBG("Global state is initialised\n");



    (void) dev;
    (void) path;
    (void) use_host_ring;
    (void) ring_id;

    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t construct_str(camio_connector_t* this, camio_uri_t* uri)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)uri;

    // TODO XXX: Convert the URI into a bunch of opts here...

    ch_cstr dev             = "eth0";
    ch_cstr path            = "/dev/netmap";
    ch_bool use_host_ring   = false;
    ch_word ring_id         = 0;

    return construct(this,dev,path,use_host_ring,ring_id);
}

static camio_error_t construct_bin(camio_connector_t* this, va_list args)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)args;

    // TODO XXX: Convert the va_list into a bunch of opts here

    ch_cstr dev             = "eth0";
    ch_cstr path            = "/dev/netmap";
    ch_bool use_host_ring   = false;
    ch_word ring_id         = 0;

    return construct(this,dev,path,use_host_ring,ring_id);

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

