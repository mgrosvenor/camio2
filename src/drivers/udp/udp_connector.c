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




#define CAMIO_UDP_MAX_STR_ARG 1024
typedef struct udp_priv_s {

    //Parameters used when a connection happens
    char rd_address[CAMIO_UDP_MAX_STR_ARG];
    char wr_address[CAMIO_UDP_MAX_STR_ARG];
    char rd_port[CAMIO_UDP_MAX_STR_ARG];
    char wr_port[CAMIO_UDP_MAX_STR_ARG];

    //Local state
    int fd;

} udp_priv_t;




static camio_error_t udp_construct(camio_connector_t* this, ch_cstr rd_address, ch_cstr rd_port, ch_cstr wr_address,
        ch_cstr wr_port)
{

    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    strncpy(priv->rd_address,rd_address,CAMIO_UDP_MAX_STR_ARG);
    strncpy(priv->wr_address,wr_address,CAMIO_UDP_MAX_STR_ARG);
    strncpy(priv->rd_port,rd_port,CAMIO_UDP_MAX_STR_ARG);
    strncpy(priv->wr_port,wr_port,CAMIO_UDP_MAX_STR_ARG);

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

    // TODO XXX: Convert the URI into a bunch of opts here... need to and add a parser like chaste options.

    ch_cstr rd_address  = "127.0.0.1"; //TODO Should add proper resolution handler for domain names
    ch_cstr wr_address  = "127.0.0.1"; //TODO Should add proper resolution handler for domain names
    ch_cstr rd_port     = "3000";
    ch_cstr wr_port     = "4000";

    return udp_construct(this,rd_address,rd_port,wr_address,wr_port);

}




static camio_error_t udp_construct_bin(camio_connector_t* this, va_list args)
{
    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)args;

    // TODO XXX: Convert the va_list into a bunch of opts here

    ch_cstr rd_address  = "127.0.0.1"; //TODO Should add proper resolution handler for domain names
    ch_cstr wr_address  = "127.0.0.1"; //TODO Should add proper resolution handler for domain names
    ch_cstr rd_port     = "3000";
    ch_cstr wr_port     = "4000";

    return udp_construct(this,rd_address,rd_port,wr_address,wr_port);

}




static camio_error_t udp_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    (void)priv;

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
