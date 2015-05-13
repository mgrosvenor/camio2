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

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>




#define CAMIO_UDP_MAX_ADDR_STR 1024
#define CAMIO_UDP_MAX_PROT_STR 10
typedef struct udp_priv_s {

    //Parameters used when a connection happens
    char rd_address[CAMIO_UDP_MAX_ADDR_STR];
    char wr_address[CAMIO_UDP_MAX_ADDR_STR];
    char rd_prot[CAMIO_UDP_MAX_PROT_STR];
    char wr_prot[CAMIO_UDP_MAX_PROT_STR];

    //Local state for each socket
    int rd_fd;
    int wr_fd;

} udp_connector_priv_t;




static camio_error_t udp_construct(camio_connector_t* this, ch_cstr rd_address, ch_cstr rd_prot, ch_cstr wr_address,
        ch_cstr wr_prot)
{

    udp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    //Do I really need to save this? I could do the connect here? Hmm. Not as neat, but will perform better
    strncpy(priv->rd_address,rd_address,CAMIO_UDP_MAX_ADDR_STR);
    strncpy(priv->wr_address,wr_address,CAMIO_UDP_MAX_ADDR_STR);
    strncpy(priv->rd_prot,rd_prot,CAMIO_UDP_MAX_PROT_STR);
    strncpy(priv->wr_prot,wr_prot,CAMIO_UDP_MAX_PROT_STR);

    return CAMIO_ENOERROR;

}


static camio_error_t udp_construct_str(camio_connector_t* this, camio_uri_t* uri)
{
//    udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
//    (void)priv;
    (void)uri;

    // TODO XXX: Convert the URI into a bunch of opts here... need to and add a parser like chaste options.

    ch_cstr rd_address  = "localhost"; //TODO Should add proper resolution handler for domain names
    ch_cstr wr_address  = "127.0.0.1"; //TODO Should add proper resolution handler for domain names
    ch_cstr rd_prot     = "3000";
    ch_cstr wr_prot     = "4000";

    return udp_construct(this,rd_address,rd_prot,wr_address,wr_prot);

}




static camio_error_t udp_construct_bin(camio_connector_t* this, va_list args)
{
    //udp_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //(void)priv;
    (void)args;

    // TODO XXX: Convert the va_list into a bunch of opts here

    ch_cstr rd_address  = "www.google.com"; //TODO Should add proper resolution handler for domain names
    ch_cstr wr_address  = "localhost"; //TODO Should add proper resolution handler for domain names
    ch_cstr rd_prot     = "dns";
    ch_cstr wr_prot     = "dns";

    return udp_construct(this,rd_address,rd_prot,wr_address,wr_prot);

}




static void udp_destroy(camio_connector_t* this)
{
    udp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    free(priv);
}

static camio_error_t resolve_bind_connect(char* address, char* prot, ch_bool do_bind, ch_bool do_connect,
        int* socket_fd_out)
{
    struct addrinfo hints, *res, *res0;
    int s;
    char* cause;
    int error;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; //This could be more general in the future if the code is moved out of the UDP stream
    error = getaddrinfo(address, prot, &hints, &res0);
    if (error) {
        DBG("Getaddrinfo() failed: %s\n", gai_strerror(error));
        return CAMIO_EBADOPT;
    }
    s = -1;
    for (res = res0; res; res = res->ai_next) {
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s < 0) {
            cause = "socket";
            continue;
        }

        if(do_connect){
            if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
                cause = "connect";
                close(s);
                s = -1;
                continue;
            }
        }

        if(do_bind){
            if (bind(s, res->ai_addr, res->ai_addrlen) < 0) {
                cause = "connect";
                close(s);
                s = -1;
                continue;
            }
        }

        break;  /* okay we got one */
    }
    if (s < 0) {
        DBG("Socket failed: %s\n", cause);
        return CAMIO_EINVALID;
    }

    //If we get here, s is populated with something meaningful
    *socket_fd_out = s;

    return CAMIO_ENOERROR;
}


static camio_error_t udp_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    udp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    if(priv->rd_fd || priv->wr_fd){
        return CAMIO_EALLREADYCONNECTED; //Process is already connected
    }


    camio_stream_t* stream = NEW_STREAM(udp);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;
    DBG("Stream address=%p (%p)\n",stream, *stream_o);

    //Parse up the address and port/protocol
    if(priv->rd_address && priv->rd_prot){
        resolve_bind_connect(priv->rd_address,priv->rd_prot,true,false, &priv->rd_fd);
    }

    if(priv->wr_address && priv->wr_prot){
        resolve_bind_connect(priv->wr_address,priv->wr_prot,false, true, &priv->wr_fd);
    }

    return udp_stream_construct(stream, this, priv->rd_fd, priv->wr_fd);
}




NEW_CONNECTOR_DEFINE(udp, udp_connector_priv_t)
