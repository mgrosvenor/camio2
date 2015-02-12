/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_connector.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "../../transports/connector.h"
#include "../../camio.h"
#include "../../camio_debug.h"

#include "udp_connector.h"
#include "udp_stream.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <memory.h>




typedef struct udp_priv_s {

} udp_priv_t;




static camio_error_t udp_construct(camio_connector_t* this, ch_cstr dev, ch_cstr path, ch_bool sw_ring, ch_bool hw_ring,
        ch_word ring_id)
{

    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    //Not much to do here. The connector doesn't have much internal state that it needs.

    //TODO Should think about making a copy here since these strings could go away between now and when we want to use them
//    priv->dev       = dev;
//    priv->path      = path;
//    priv->sw_ring   = sw_ring;
//    priv->hw_ring   = hw_ring;
//    priv->ring_id   = ring_id;

    return CAMIO_ENOERROR;

}




static void udp_destroy(camio_connector_t* this)
{
    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    free(priv);
}




static camio_error_t udp_construct_str(camio_connector_t* this, camio_uri_t* uri)
{
    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)uri;

    // TODO XXX: Convert the URI into a bunch of opts here...

    ch_cstr dev       = "eth0";
    ch_cstr path      = "/dev/udp";
    ch_bool sw_ring   = false;
    ch_bool hw_ring   = true;
    ch_word ring_id   = 0;

    return udp_construct(this,dev,path,sw_ring,hw_ring, ring_id);
}




static camio_error_t udp_construct_bin(camio_connector_t* this, va_list args)
{
    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)args;

    // TODO XXX: Convert the va_list into a bunch of opts here

    ch_cstr dev             = "eth0";
    ch_cstr path            = "/dev/udp";
    ch_bool sw_ring   = false;
    ch_bool hw_ring   = true;
    ch_word ring_id   = 0;

    return udp_construct(this,dev,path,sw_ring,hw_ring, ring_id);

}




static camio_error_t udp_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    int udp_fd = -1;


    camio_stream_t* stream = NEW_STREAM(udp);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;
    DBG("Stream address=%p (%p)\n",stream, *stream_o);


    return udp_stream_construct(stream,udp_fd);
}




NEW_CONNECTOR_DEFINE(udp, udp_priv_t)
