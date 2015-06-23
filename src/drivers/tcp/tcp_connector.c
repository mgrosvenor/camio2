/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   22 Jun 2015
 *  File name: tcp_connector.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "../../transports/connector.h"
#include "../../camio.h"
#include "../../camio_debug.h"

#include <src/buffers/buffer_malloc_linear.h>

#include "tcp_transport.h"
#include "tcp_connector.h"
#include "tcp_stream.h"

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


/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/
#define CAMIO_TCP_MAX_ADDR_STR 1024
#define CAMIO_TCP_MAX_PROT_STR 10
typedef struct tcp_priv_s {

    //Parameters used when a connection happens
    tcp_params_t* params;

    len_string_t address;
    len_string_t protocol;

    //Has connect be called?
    int con_fd_tmp;


} tcp_connector_priv_t;


/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

static camio_error_t resolve_bind_connect(char* address, char* prot, ch_bool do_bind, ch_bool do_connect,
        int* socket_fd_out)
{
    struct addrinfo hints, *res, *res0;
    int s;
    char* cause;
    int error;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; //This could be more general in the future if the code is moved out of the TCP stream
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
            DBG("Doing connect\n");
            if (connect(s, res->ai_addr, res->ai_addrlen) < 0) {
                cause = "connect";
                close(s);
                s = -1;
                continue;
            }
        }


        if(do_bind){
            DBG("Doing bind addrelen=%i\n", res->ai_addrlen);
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

//Try to see if connecting is possible. With TCP, it is always possible.
static camio_error_t tcp_connect_peek(camio_connector_t* this)
{
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    if(this->muxable.fd < 0){ //No socket yet
        const bool do_bind    = priv->params->listen;
        const bool do_connect = !do_bind;
        camio_error_t err = resolve_bind_connect(priv->address.str,priv->protocol.str,do_bind,do_connect, &this->muxable.fd);
        if(err != CAMIO_ENOERROR){
            return err; //We cannot connect something went wrong. //TODO XXX better error code
        }

        //We got a valid FD, set it to non-block
        int flags = fcntl(this->muxable.fd, F_GETFL, 0);
        fcntl(this->muxable.fd, F_SETFL, flags | O_NONBLOCK);

        //Mark the socket as listening if needed
        if(priv->params->listen){
           if(listen(this->muxable.fd,1024)){
               DBG("Failed to listen with error %s\n", strerror(errno));
               return CAMIO_ECHECKERRORNO;
           }
        }
    }
    //OK, now that we've set up a non-blocking descriptor, we can go and accept or connect
    if(!priv->params->listen){
        priv->con_fd_tmp = this->muxable.fd;
        return CAMIO_ENOERROR; //We're connected off we go!
    }

    //Time to accept now incoming connections
    //struct sockaddr_in client_addr;
    //socklen_t client_addr_len = sizeof(struct sockaddr_in);
    //priv->con_fd_tmp = accept(this->muxable.fd,(struct sockaddr *)&client_addr,&client_addr_len);
    priv->con_fd_tmp = accept(this->muxable.fd,NULL,NULL);
    if(priv->con_fd_tmp < 0){
        if(errno == EWOULDBLOCK || errno == EAGAIN){
            return CAMIO_ETRYAGAIN;
        }
        else{
            DBG("Something else went wrong, check errno value (%s)\n",strerror(errno));
            return CAMIO_ECHECKERRORNO;
        }
    }

    //Woot connection accepted!

    return CAMIO_ENOERROR;
}

static camio_error_t tcp_connector_ready(camio_muxable_t* this)
{
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.connector);

    if(priv-> con_fd_tmp > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = tcp_connect_peek(this->parent.connector);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t tcp_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = tcp_connect_peek(this);
    if(err != CAMIO_ENOERROR){
        return err;
    }
    //DBG("Done connecting, now constructing TCP stream...\n");

    camio_stream_t* stream = NEW_STREAM(tcp);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = tcp_stream_construct(stream, this, priv->con_fd_tmp);
    if(err){
       return err;
    }

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t tcp_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(tcp_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    tcp_params_t* tcp_params = (tcp_params_t*)(*params);
    DBG("Constructing TCP with parameters: hier=%s, listen=%i\n",
            tcp_params->hierarchical.str,
            tcp_params->listen
    );


    //We require a hierarchical part!
    if(tcp_params->hierarchical.str_len == 0){
        DBG("Expecting a hierarchical part in the TCP URI, but none was given. e.g tcp:localhost:2000\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }

    //OK. we've got one, go looking for a protocol mark
    const char* protocol_mark   = strchr(tcp_params->hierarchical.str, ':');
    const ch_word protocol_len = tcp_params->hierarchical.str_len - (protocol_mark - tcp_params->hierarchical.str + 1);
    ch_word address_len  = 0;
    if(!protocol_mark){
        DBG("Could not find a protocol mark! Expected to find \":\" in the address\n");
        return CAMIO_EINVALID; //TODO XXX: Need a better error value
    }

    address_len = protocol_mark - tcp_params->hierarchical.str ;
    DBG("Protocol len = %i address len = %i\n", protocol_len, address_len);

    //Copy the addresses and protocol
    strncpy(priv->address.str, tcp_params->hierarchical.str, address_len );
    priv->address.str_len = address_len;
    strncpy(priv->protocol.str, protocol_mark + 1, protocol_len );
    priv->protocol.str_len = protocol_len;

    //Populate the parameters
    priv->params                    = tcp_params;

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = tcp_connector_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void tcp_destroy(camio_connector_t* this)
{
    DBG("Destorying tcp connector\n");
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    //Only close this if it's in listening mode and the streams will not be affected
    if(this->muxable.fd > -1 && priv->params->listen)  { close(this->muxable.fd); }
    DBG("Freed FD's\n");

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed connector structure\n");
}

NEW_CONNECTOR_DEFINE(tcp, tcp_connector_priv_t)
