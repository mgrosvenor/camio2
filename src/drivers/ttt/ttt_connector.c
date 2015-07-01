/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_connector.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <src/transports/connector.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include "ttt_transport.h"
#include "ttt_connector.h"
#include "ttt_stream.h"



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/
typedef struct ttt_priv_s {
    ch_word temp;
} ttt_connector_priv_t;


/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

//Try to see if connecting is possible.
static camio_error_t ttt_connect_peek(camio_connector_t* this)
{
    DBG("Doing TTT connect peek\n");

    return CAMIO_ETRYAGAIN;
    return CAMIO_ECHECKERRORNO;
    return CAMIO_ENOERROR;
}

static camio_error_t ttt_connector_ready(camio_muxable_t* this)
{
    DBG("Checking if TTT is ready to connect...\n");
    ttt_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.connector);


    return CAMIO_ENOTREADY;
    return CAMIO_EREADY;
}

static camio_error_t ttt_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    ttt_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = ttt_connector_ready(&this->muxable);
    if(err != CAMIO_EREADY){
        return err;
    }
    DBG("Done connecting, now constructing TTT stream...\n");

    camio_stream_t* stream = NEW_STREAM(ttt);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = ttt_stream_construct(stream, this /*....*/);
    if(err){
       return err;
    }

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t ttt_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    ttt_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(ttt_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    ttt_params_t* ttt_params = (ttt_params_t*)(*params);

    //Populate the parameters

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = ttt_connector_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void ttt_destroy(camio_connector_t* this)
{
    DBG("Destorying ttt connector\n");
    ttt_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed connector structure\n");
}

NEW_CONNECTOR_DEFINE(ttt, ttt_connector_priv_t)
