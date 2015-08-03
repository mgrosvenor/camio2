/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   01 Jul 2015
 *  File name: mfio_device.h
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

#include <src/devices/device.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include "mfio_device.h"
#include "mfio_device.h"
#include "mfio_channel.h"


/**************************************************************************************************************************
 * PER CHANNEL STATE
 **************************************************************************************************************************/
typedef struct mfio_priv_s {

    mfio_params_t* params;  //Parameters used when a connection happens

    bool is_devected;          //Has devect be called?
    void* base_mem_start;             //Size and location of the mmaped region
    ch_word base_mem_len;         //
    int base_fd;                //File descriptor for the backing buffer

} mfio_device_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

//Try to see if devecting is possible. With UDP, it is always possible.
static camio_error_t mfio_devect_peek(camio_device_t* this)
{
    DBG("Doing devect peek\n");
    mfio_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    if(priv->base_fd > -1 ){
        return CAMIO_ENOERROR; //Ready to go, please call devect!
    }

    //Open up the file and get it ready to devect
    priv->base_fd = open(priv->params->hierarchical.str, O_RDWR);
    if(priv->base_fd < 0){
        DBG("Could not open file \"%s\". Error=%s\n", priv->params->hierarchical.str, strerror(errno));
        return CAMIO_EINVALID;
    }

    //Get the file size
    struct stat st;
    stat(priv->params->hierarchical.str, &st);
    priv->base_mem_len = st.st_size;
    DBG("Got file size of %lli\n", priv->base_mem_len);


    DBG("Mapping new file with size %lli\n", priv->base_mem_len);

    //Map the whole thing into memory
    priv->base_mem_start = mmap( NULL, priv->base_mem_len, PROT_READ | PROT_WRITE, MAP_SHARED, priv->base_fd, 0);
    if(priv->base_mem_start == MAP_FAILED){
        DBG("Could not memory map blob file \"%s\". Error=%s\n", priv->params->hierarchical, strerror(errno));
        return CAMIO_ECHECKERRORNO; //TODO XXX better errors
    }

    DBG("Mapped file to address %p with size %lli\n", priv->base_mem_start, priv->base_mem_len);

    //printf("%.*s\n",(int)priv->base_mem_len, priv->base_mem_start );

    return CAMIO_ENOERROR;

}

static camio_error_t mfio_device_ready(camio_muxable_t* this)
{
    if(this->fd > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = mfio_devect_peek(this->parent.device);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t mfio_devect(camio_device_t* this, camio_channel_t** channel_o )
{
    mfio_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    camio_error_t err = mfio_devect_peek(this);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    if(priv->is_devected){
        DBG("Already devected! Why are you calling this twice?\n");
        return CAMIO_EALLREADYCONNECTED; // We're already devected!
    }

    //DBG("Done devecting, now constructing UDP channel...\n");
    camio_channel_t* channel = NEW_CHANNEL(mfio);
    if(!channel){
        *channel_o = NULL;
        return CAMIO_ENOMEM;
    }
    *channel_o = channel;

    err = mfio_channel_construct(channel, this, priv->base_fd, priv->base_mem_start, priv->base_mem_len);
    if(err){
       return err;
    }

    priv->is_devected = true;
    return CAMIO_ENOERROR;
}



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t mfio_construct(camio_device_t* this, void** params, ch_word params_size)
{

    mfio_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
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

    priv->base_fd = -1;

    //Populate the rest of the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.device  = this;
    this->muxable.vtable.ready      = mfio_device_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void mfio_destroy(camio_device_t* this)
{
    DBG("Destorying mfio device\n");
    mfio_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed device structure\n");
}

NEW_DEVICE_DEFINE(mfio, mfio_device_priv_t)

