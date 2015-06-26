/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_connector.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "../../transports/connector.h"
#include "../../camio.h"
#include "../../camio_debug.h"

#include "ttt_transport.h"
#include "ttt_connector.h"
#include "ttt_stream.h"


/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct ttt_priv_s {
    //Parameters used when a connection happens
    ttt_params_t* params;

} ttt_connector_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/
//Try to see if connecting is possible. With TTT, it is always possible.
static camio_error_t ttt_connect_peek(ttt_connector_priv_t* priv)
{
    (void)priv;
    return CAMIO_NOTIMPLEMENTED;
}

static camio_error_t ttt_connector_ready(camio_muxable_t* this)
{
    ttt_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.connector);

//    if(priv->){
//        return CAMIO_EREADY;
//    }

    camio_error_t err = ttt_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t ttt_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    ttt_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = ttt_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    camio_stream_t* stream = NEW_STREAM(ttt);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = ttt_stream_construct(stream, this /**, , , **/);
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
        return CAMIO_EINVALID; //TODO TTT : Need better error values
    }
    ttt_params_t* ttt_params = (ttt_params_t*)(*params);

    /// .... check and parse the parameters here

    //Populate the parameters
    priv->params                    = ttt_params;

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = ttt_connector_ready;
    this->muxable.fd                = -1;

    //Populate private state
    //priv->... =  ...;

    return CAMIO_ENOERROR;
}


static void ttt_destroy(camio_connector_t* this)
{
    DBG("Destroying ttt connector...\n");
    ttt_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    //Free local stuff
    //    if(priv->...)  { close(...); }
    //    if(priv->...)  { free(...); }

    if(priv->params) { free(priv->params); }
    free(this);
    DBG("Done destroying ttt connector structure\n");
}

NEW_CONNECTOR_DEFINE(ttt, ttt_connector_priv_t)
