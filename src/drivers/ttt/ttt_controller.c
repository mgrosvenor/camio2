/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_controller.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <src/devices/controller.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include "ttt_device.h"
#include "ttt_controller.h"
#include "ttt_channel.h"



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/
typedef struct ttt_priv_s {
    ch_word temp;
} ttt_controller_priv_t;


/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

//Try to see if connecting is possible.
static camio_error_t ttt_connect_peek(camio_controller_t* this)
{
    DBG("Doing TTT connect peek\n");

    return CAMIO_ETRYAGAIN;
    return CAMIO_ECHECKERRORNO;
    return CAMIO_ENOERROR;
}

static camio_error_t ttt_controller_ready(camio_muxable_t* this)
{
    DBG("Checking if TTT is ready to connect...\n");
    ttt_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.controller);


    return CAMIO_ENOTREADY;
    return CAMIO_EREADY;
}

static camio_error_t ttt_connect(camio_controller_t* this, camio_channel_t** channel_o )
{
    ttt_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = ttt_controller_ready(&this->muxable);
    if(err != CAMIO_EREADY){
        return err;
    }
    DBG("Done connecting, now constructing TTT channel...\n");

    camio_channel_t* channel = NEW_STREAM(ttt);
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

static camio_error_t ttt_construct(camio_controller_t* this, void** params, ch_word params_size)
{

    ttt_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(ttt_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    ttt_params_t* ttt_params = (ttt_params_t*)(*params);

    //Populate the parameters

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.controller  = this;
    this->muxable.vtable.ready      = ttt_controller_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void ttt_destroy(camio_controller_t* this)
{
    DBG("Destorying ttt controller\n");
    ttt_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed controller structure\n");
}

NEW_CONNECTOR_DEFINE(ttt, ttt_controller_priv_t)
