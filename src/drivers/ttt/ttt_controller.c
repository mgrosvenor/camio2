/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <src/devices/device.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include "ttt_device.h"
#include "ttt_device.h"
#include "ttt_channel.h"



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/
typedef struct ttt_priv_s {
    ch_word temp;
} ttt_device_priv_t;


/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

//Try to see if connecting is possible.
static camio_error_t ttt_connect_peek(camio_dev_t* this)
{
    DBG("Doing TTT connect peek\n");

    return CAMIO_ETRYAGAIN;
    return CAMIO_ECHECKERRORNO;
    return CAMIO_ENOERROR;
}

static camio_error_t ttt_device_ready(camio_muxable_t* this)
{
    DBG("Checking if TTT is ready to connect...\n");
    ttt_device_priv_t* priv = DEVICE_GET_PRIVATE(this->parent.dev);


    return CAMIO_ENOTREADY;
    return CAMIO_EREADY;
}

static camio_error_t ttt_connect(camio_dev_t* this, camio_channel_t** channel_o )
{
    ttt_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    camio_error_t err = ttt_device_ready(&this->muxable);
    if(err != CAMIO_EREADY){
        return err;
    }
    DBG("Done connecting, now constructing TTT channel...\n");

    camio_channel_t* channel = NEW_CHANNEL(ttt);
    if(!channel){
        *channel_o = NULL;
        return CAMIO_ENOMEM;
    }
    *channel_o = channel;

    err = ttt_channel_construct(channel, this /*....*/);
    if(err){
       return err;
    }

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t ttt_construct(camio_dev_t* this, void** params, ch_word params_size)
{

    ttt_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(ttt_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    ttt_params_t* ttt_params = (ttt_params_t*)(*params);

    //Populate the parameters

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.dev  = this;
    this->muxable.vtable.ready      = ttt_device_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void ttt_destroy(camio_dev_t* this)
{
    DBG("Destorying ttt device\n");
    ttt_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed device structure\n");
}

NEW_DEVICE_DEFINE(ttt, ttt_device_priv_t)
