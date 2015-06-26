/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_connector.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "../../transports/connector.h"
#include "../../camio.h"
#include "../../camio_debug.h"

#include "delim_transport.h"
#include "delim_connector.h"
#include "delim_stream.h"


/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct delim_priv_s {
    //Parameters used when a connection happens
    delim_params_t* params;

} delim_connector_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/
//Try to see if connecting is possible. With DELIM, it is always possible.
static camio_error_t delim_connect_peek(delim_connector_priv_t* priv)
{
    (void)priv;
    return CAMIO_NOTIMPLEMENTED;
}

static camio_error_t delim_connector_ready(camio_muxable_t* this)
{
    delim_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.connector);

//    if(priv->){
//        return CAMIO_EREADY;
//    }

    camio_error_t err = delim_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t delim_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    delim_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = delim_connect_peek(priv);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    camio_stream_t* stream = NEW_STREAM(delim);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = delim_stream_construct(stream, this /**, , , **/);
    if(err){
       return err;
    }

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t delim_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    delim_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(delim_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO DELIM : Need better error values
    }
    delim_params_t* delim_params = (delim_params_t*)(*params);

    /// .... check and parse the parameters here

    //Populate the parameters
    priv->params                    = delim_params;

    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = delim_connector_ready;
    this->muxable.fd                = -1;

    //Populate private state
    //priv->... =  ...;

    return CAMIO_ENOERROR;
}


static void delim_destroy(camio_connector_t* this)
{
    DBG("Destroying delim connector...\n");
    delim_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    //Free local stuff
    //    if(priv->...)  { close(...); }
    //    if(priv->...)  { free(...); }

    if(priv->params) { free(priv->params); }
    free(this);
    DBG("Done destroying delim connector structure\n");
}

NEW_CONNECTOR_DEFINE(delim, delim_connector_priv_t)
