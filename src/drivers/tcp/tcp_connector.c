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

    ch_bool connected_client;


} tcp_connector_priv_t;


/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/
#define TCP_ERROR_NONE      0
#define TCP_ERROR_SOCKET    1
#define TCP_ERROR_CONNECT   2
#define TCP_ERROR_BIND      3
static char* tcp_error_strs[] = {
        "No error",
        "Socket error",
        "Connect error",
        "Bind error",
};

static camio_error_t resolve_bind_connect(char* address, char* prot, ch_bool do_bind, ch_bool do_connect,
        int* socket_fd_out)
{
    struct addrinfo hints       = {0};
    struct addrinfo *res_head   = NULL;
    int error                   = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM; //This could be more general in the future if the code is moved out of the TCP stream
    error = getaddrinfo(address, prot, &hints, &res_head);
    if (error) {
        ERR("Getaddrinfo() failed: %s\n", gai_strerror(error));
        return CAMIO_EBADOPT;
    }
    if(!res_head){//Not sure if this is even possible given the error check above?
        ERR("Resolving address resulted in no options. Bad address??\n");
        return CAMIO_EBADOPT;
    }

    //Iterate over all results, see if we can connect or bind to any of them
    ch_word soc_fd = -1;
    #define IP_STR_MAX (64)
    char str_to_ip[IP_STR_MAX] = {0};

    for ( struct addrinfo *res = res_head; res; res = res->ai_next) {
        inet_ntop(res->ai_family,res->ai_addr,&str_to_ip[0],IP_STR_MAX);

        soc_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (soc_fd < 0) {
            error = TCP_ERROR_SOCKET;
            continue;
        }

        if(do_connect){
            DBG("Doing TCP connect to (%s:%s) %s...\n",address, prot, str_to_ip);
            if (connect(soc_fd, res->ai_addr, res->ai_addrlen) < 0) {
                error = TCP_ERROR_CONNECT;
                close(soc_fd);
                soc_fd = -1;
                continue;
            }
        }


        if(do_bind){
            DBG("Doing TCP bind to (%s:%s) %s...\n",address, prot,str_to_ip );
            if (bind(soc_fd, res->ai_addr, res->ai_addrlen) < 0) {
                error = TCP_ERROR_BIND;
                close(soc_fd);
                soc_fd = -1;
                continue;
            }
        }

        break;  /* okay we got one */
    }
    if (soc_fd < 0) {
        if(error == TCP_ERROR_CONNECT){
            //Nothing to connect to, this is probably not fatal
            return CAMIO_ENOTREADY;
        }

        //Other errors probably are fatal
        const char* error_str = tcp_error_strs[error];
        (void)error_str;
        ERR("Creating socket failed: %s\n", error_str);
        return CAMIO_EINVALID; //OK. We were not ready to connect for some reason. TODO XXX better error code here
    }

    //If we get here, soc_fd is populated with something meaningful! yay!
    *socket_fd_out = soc_fd;

    DBG("Done %s to address %s with protocol %s\n", do_bind ? "binding" : "connecting", address, prot);

    return CAMIO_ENOERROR;
}

//Try to see if connecting is possible. With TCP, it is always possible.
static camio_error_t tcp_connect_peek(camio_connector_t* this)
{

    DBG("Doing TCP connect peek\n");
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    if(this->muxable.fd < 0){ //No socket yet
        const bool do_bind    = priv->params->listen;
        const bool do_connect = !do_bind;
        camio_error_t err = resolve_bind_connect(priv->address.str,priv->protocol.str,do_bind,do_connect, &this->muxable.fd);
        if(err == CAMIO_ENOTREADY){
            DBG("TCP is not ready to connect, but it might be in the future\n");
            return CAMIO_ETRYAGAIN;
        }
        if(err != CAMIO_ENOERROR){
            return err; //We cannot connect something went wrong. //TODO XXX better error code
        }

        //We got a valid FD, set it to non-block
        int flags = fcntl(this->muxable.fd, F_GETFL, 0);
        fcntl(this->muxable.fd, F_SETFL, flags | O_NONBLOCK);

        //Mark the socket as listening if needed
        if(priv->params->listen){
           if(listen(this->muxable.fd,1024)){
               ERR("Failed to listen with error %s\n", strerror(errno));
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
            ERR("Something else went wrong, check errno value (%s)\n",strerror(errno));
            return CAMIO_ECHECKERRORNO;
        }
    }

    //Woot connection accepted!

    return CAMIO_ENOERROR;
}

static camio_error_t tcp_connector_ready(camio_muxable_t* this)
{
    DBG("Checking if TCP is ready to connect...\n");
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.connector);

    if(priv->connected_client){
        ERR("Already connected!\n");
        return CAMIO_ENOTREADY;
    }

    if(priv->con_fd_tmp > -1){
        ERR("Ready to connect, FD is > -1\n");
        return CAMIO_EREADY;
    }

    camio_error_t err = tcp_connect_peek(this->parent.connector);
    if(err == CAMIO_ETRYAGAIN){
        DBG("Not ready to connect, try again in a while\n");
        return CAMIO_ENOTREADY;
    }
    if(err != CAMIO_ENOERROR){
        DBG("Not ready to connect, Some other error (%i)\n", err);
        return err;
    }

    DBG("Ready to connect, FD is now %i\n", priv->con_fd_tmp);
    return CAMIO_EREADY;
}

static camio_error_t tcp_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    tcp_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = tcp_connector_ready(&this->muxable);
    if(err != CAMIO_EREADY){
        return err;
    }
    DBG("Done connecting, now constructing TCP stream...\n");

    camio_stream_t* stream = NEW_STREAM(tcp);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = tcp_stream_construct(stream, this,priv->params, priv->con_fd_tmp);
    if(err){
       return err;
    }


    if(!priv->params->listen){ //Make sure we only connect once on a client
        //DBG("Only connect once!\n");
        priv->connected_client = true;
    }
    priv->con_fd_tmp = -1;

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
        ERR("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    tcp_params_t* tcp_params = (tcp_params_t*)(*params);
    DBG("Constructing TCP with parameters: hier=%s, listen=%i\n",
            tcp_params->hierarchical.str,
            tcp_params->listen
    );


    //We require a hierarchical part!
    if(tcp_params->hierarchical.str_len == 0){
        ERR("Expecting a hierarchical part in the TCP URI, but none was given. e.g tcp:localhost:2000\n");
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
    priv->con_fd_tmp                = -1;

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
