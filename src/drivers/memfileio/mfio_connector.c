/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   01 Jul 2015
 *  File name: mfio_connector.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <sys/stat.h>

#include <src/transports/connector.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include "mfio_transport.h"
#include "mfio_connector.h"
#include "mfio_stream.h"


/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
#define CAMIO_UDP_MAX_ADDR_STR 1024
#define CAMIO_UDP_MAX_PROT_STR 10
typedef struct mfio_priv_s {

    mfio_params_t* params;  //Parameters used when a connection happens

    bool is_connected;          //Has connect be called?
    camio_buffer_t mmap_buff;   //Buffer descriptor for mmaped version of base_fd;
    int base_fd;                //File descriptor for the backing buffer

} mfio_connector_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

//Try to see if connecting is possible. With UDP, it is always possible.
static camio_error_t mfio_connect_peek(camio_connector_t* this)
{
    mfio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->is_connected){
        return CAMIO_EALLREADYCONNECTED; // We're already connected!
    }

    if(priv->base_fd > -1 ){
        return CAMIO_ENOERROR; //Ready to go, please call connect!
    }

    //Get the file size
    struct stat st;
    stat(priv->params->hierarchical, &st);
    priv->mmap_buff.buffer_len = st.st_size;

    if(priv->mmap_buff.buffer_len)
    {
        DBG("Mapping new file with size %lli\n", priv->mmap_buff.buffer_len);

        //Map the whole thing into memory
        priv->mmap_buff.buffer_start = mmap( NULL, priv->mmap_buff.buffer_len, PROT_READ, MAP_SHARED, priv->base_fd, 0);
        if(unlikely(priv->mmap_buff.buffer_len == MAP_FAILED)){
            DBG("Could not memory map blob file \"%s\". Error=%s\n", priv->params->hierarchical, strerror(errno));
            return CAMIO_ECHECKERRORNO; //TODO XXX better errors
        }

        priv->is_connected = 1;
        return CAMIO_ENOERROR;
    }

    DBG("file has zero size\n");
    return CAMIO_EINVALID;
}

static camio_error_t mfio_connector_ready(camio_muxable_t* this)
{
    mfio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.connector);

    if(this->fd > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = mfio_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t mfio_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    mfio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = mfio_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    if(priv->is_connected){
        return CAMIO_EINVALID;//Only allow one connection
    }

    //DBG("Done connecting, now constructing UDP stream...\n");
    camio_stream_t* stream = NEW_STREAM(mfio);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = mfio_stream_construct(stream, this, priv->blob, priv->base_fd, priv->mmap_buff);
    if(err){
       return err;
    }

    priv->is_connected = true;
    return CAMIO_ENOERROR;
}



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t mfio_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    mfio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(mfio_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    mfio_params_t* mfio_params = (mfio_params_t*)(*params);
    priv->params = mfio_params;

    //We require a hierarchical part!
    if(mfio_params->hierarchical.str_len == 0){
        DBG("Expecting a hierarchical part in the MFIO URI, but none was given. e.g /dir/myfile.blob\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }

    //Open up the file and get it ready to connect
    priv->base_fd = open(mfio_params->hierarchical, O_RDWR);
    if(unlikely(priv->base_fd < 0)){
        DBG("Could not open file \"%s\". Error=%s\n", mfio_params->hierarchical, strerror(errno));
        return CAMIO_EINVALID;
    }

    //Populate the rest of the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = mfio_connector_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void mfio_destroy(camio_connector_t* this)
{
    DBG("Destorying mfio connector\n");
    mfio_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed connector structure\n");
}

NEW_CONNECTOR_DEFINE(mfio, mfio_connector_priv_t)

