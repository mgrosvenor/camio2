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

#include <src/buffers/buffer_malloc_linear.h>

#include "udp_transport.h"
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
    udp_params_t* params;

    //Local state for each socket
    int rd_fd;
    int wr_fd;

} udp_connector_priv_t;


static camio_error_t udp_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    udp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(udp_params_t)){
        return CAMIO_EINVALID;
    }
    udp_params_t* udp_params = (udp_params_t*)(*params);
    priv->params = udp_params;
    DBG("rd_add=%i, rd_prot=%i, wr_addr=%i, wr_pro=%i\n",
            udp_params->rd_address.str_len,
            udp_params->rd_protocol.str_len,
            udp_params->wr_address.str_len,
            udp_params->wr_protocol.str_len
            );


    DBG("rd_add=%s, rd_prot=%s, wr_addr=%s, wr_pro=%s\n",
            udp_params->rd_address.str,
            udp_params->rd_protocol.str,
            udp_params->wr_address.str,
            udp_params->wr_protocol.str
            );


    //OK we're done with this now.
    free(*params);
    *params = NULL;

    return CAMIO_ENOERROR;
}


static void udp_destroy(camio_connector_t* this)
{
    udp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    if(priv->params) { free(priv->params); }
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

    DBG("Done %s to address %s with protocol %s\n", do_bind ? "binding" : "connecting", address, prot);

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
    if(priv->params->rd_address.str && priv->params->rd_protocol.str){
        resolve_bind_connect(priv->params->rd_address.str,priv->params->rd_protocol.str,true,false, &priv->rd_fd);
    }

    if(priv->params->wr_address.str && priv->params->wr_protocol.str){
        resolve_bind_connect(priv->params->wr_address.str,priv->params->wr_protocol.str,false, true, &priv->wr_fd);
    }

    DBG("Done connecting, now constructing UDP stream...\n");

    return udp_stream_construct(stream, this, priv->rd_fd, priv->wr_fd);
}




NEW_CONNECTOR_DEFINE(udp, udp_connector_priv_t)
