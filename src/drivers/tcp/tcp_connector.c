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

#define CAMIO_TCP_MAX_ADDR_STR 1024
#define CAMIO_TCP_MAX_PROT_STR 10
typedef struct tcp_priv_s {

    //Parameters used when a connection happens
    tcp_params_t* params;

    //Local state for each socket
    int rd_fd;
    int wr_fd;

    //Has connect be called?
    bool is_connected;

} tcp_connector_priv_t;


static camio_error_t resolve_bind_connect(char* address, char* prot, ch_bool do_bind, ch_bool do_connect,
        int* socket_fd_out)
{
    struct addrinfo hints, *res, *res0;
    int s;
    char* cause;
    int error;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; //This could be more general in the future if the code is moved out of the TCP stream
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

//Try to see if connecting is possible. With TCP, it is always possible.
static camio_error_t tcp_connect_peek(tcp_connector_priv_t* priv)
{
    if(priv->is_connected){
        return CAMIO_EALLREADYCONNECTED; // We're already connected!
    }

    if(priv->rd_fd > -1 || priv->wr_fd > -1){
        return CAMIO_ENOERROR; //Ready to go, please call connect!
    }

    //Parse up the address and port/protocol
    if(priv->params->rd_address.str && priv->params->rd_protocol.str){
        if(resolve_bind_connect(priv->params->rd_address.str,priv->params->rd_protocol.str,true,false, &priv->rd_fd)){
            return CAMIO_EINVALID; //We cannot connect something went wrong. //TODO XXX better error code
        }
    }

    if(priv->params->wr_address.str && priv->params->wr_protocol.str){
        if(resolve_bind_connect(priv->params->wr_address.str,priv->params->wr_protocol.str,false, true, &priv->wr_fd)){
            if(priv->rd_fd){ //Tear down the whole world.
                close(priv->rd_fd);
                priv->rd_fd = -1;
            }
            return CAMIO_EINVALID; //We cannot connect something went wrong. //TODO XXX better error code
        }
    }

    return CAMIO_ENOERROR;
}

static camio_error_t tcp_connector_ready(camio_muxable_t* this)
{
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.connector);
//    if(priv->is_connected){
//        return CAMIO_ENOTREADY;//Only allow one connection
//    }

    if(priv->rd_fd > -1 || priv->wr_fd > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = tcp_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t tcp_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = tcp_connect_peek(priv);
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

    err = tcp_stream_construct(stream, this, priv->rd_fd, priv->wr_fd);
    if(err){
       return err;
    }

    priv->is_connected = true;
    return CAMIO_ENOERROR;
}


static camio_error_t tcp_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(tcp_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    tcp_params_t* tcp_params = (tcp_params_t*)(*params);
    DBG("Constructing TCP with parameters: hier=%s, rd_add=%s, rd_prot=%s, wr_addr=%s, wr_pro=%s\n",
            tcp_params->hierarchical.str,
            tcp_params->rd_address.str,
            tcp_params->rd_protocol.str,
            tcp_params->wr_address.str,
            tcp_params->wr_protocol.str
    );

    if( tcp_params->rd_address.str_len  &&
        tcp_params->rd_protocol.str_len &&
        tcp_params->wr_address.str_len  &&
        tcp_params->wr_protocol.str_len &&
        tcp_params->hierarchical.str_len ){
        DBG("A hierarchical part was supplied, but is not needed because options were supplied too.\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }


    if( tcp_params->rd_address.str_len  == 0 ||
        tcp_params->rd_protocol.str_len == 0 ||
        tcp_params->wr_address.str_len  == 0 ||
        tcp_params->wr_protocol.str_len == 0 ){ //We're missing info that we need. See if we can get it

        //We do require a hierarchical part!
        if(tcp_params->hierarchical.str_len == 0){
            DBG("Expecting a hierarchical part in the TCP URI, but none was given. e.g tcp:localhost:2000\n");
            return CAMIO_EINVALID; //TODO XXX : Need better error values
        }

        //OK. we've got one, go looking for a protocol mark
        const char* protocol_mark   = strchr(tcp_params->hierarchical.str, ':');
        const ch_word protocol_len = tcp_params->hierarchical.str_len - (protocol_mark - tcp_params->hierarchical.str + 1);
        ch_word address_len  = 0;
        if(protocol_mark){
            address_len = protocol_mark - tcp_params->hierarchical.str ;
        }
        else{
            address_len = tcp_params->hierarchical.str_len;
        }
        DBG("Protocol len = %i address len = %i\n", protocol_len, address_len);

        //Copy the addresses if we need them
        if(tcp_params->rd_address.str_len == 0){
            strncpy(tcp_params->rd_address.str, tcp_params->hierarchical.str, address_len );
            tcp_params->rd_address.str_len = address_len;
        }

        //Copy the addresses if we need them
        if(tcp_params->wr_address.str_len == 0){
            strncpy(tcp_params->wr_address.str, tcp_params->hierarchical.str, address_len );
            tcp_params->wr_address.str_len = address_len;
        }

        //Copy the protocols if we have them
        if( (tcp_params->rd_protocol.str_len == 0 || tcp_params->wr_protocol.str_len == 0) && protocol_mark == NULL){
            DBG("Expecting a protocol mark in the TCP URI, but none was given. e.g tcp:localhost:2000\n");
            return CAMIO_EINVALID; //TODO XXX : Need better error values
        }

        if(tcp_params->rd_protocol.str_len == 0){
            strncpy(tcp_params->rd_protocol.str, protocol_mark + 1, protocol_len );
            tcp_params->rd_protocol.str_len = address_len;
        }

        if(tcp_params->wr_protocol.str_len == 0){
            strncpy(tcp_params->wr_protocol.str, protocol_mark + 1, protocol_len );
            tcp_params->wr_protocol.str_len = address_len;
        }
    }

    //Populate the parameters
    priv->params                    = tcp_params;

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = tcp_connector_ready;
    this->muxable.fd                = -1;

    //Populate the descriptors
    priv->rd_fd = -1;
    priv->wr_fd = -1;

    return CAMIO_ENOERROR;
}


static void tcp_destroy(camio_connector_t* this)
{
    DBG("Destorying tcp connector\n");
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

// Don't free these! The stream relies on them!!
//    if(priv->rd_fd)  { close(priv->rd_fd); }
//    if(priv->wr_fd)  { close(priv->wr_fd); }
//    DBG("Freed FD's\n");

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed connector structure\n");
}

NEW_CONNECTOR_DEFINE(tcp, tcp_connector_priv_t)
