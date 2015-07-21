/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_controller.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "../../devices/controller.h"
#include "../../camio.h"
#include "../../camio_debug.h"

#include "delim_device.h"
#include "delim_controller.h"
#include "delim_channel.h"


/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct delim_priv_s {
    //Parameters used when a connection happens
    delim_params_t* params;

    //Properties of the base channel
    camio_controller_t* base;
    void* base_params;
    ch_word base_params_size;
    ch_word base_id;

} delim_controller_priv_t;




/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/
static camio_error_t delim_controller_ready(camio_muxable_t* this)
{
//    DBG("Checking if base controller is ready\n");
    //Forward the ready function from the base controller. The delimiter is always ready to party!
    delim_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this->parent.controller);
    return camio_controller_ready(priv->base);

}

static camio_error_t delim_connect(camio_controller_t* this, camio_channel_t** channel_o )
{
    delim_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_channel_t* base_channel;
    DBG("Doing connect on base controller\n");
    camio_error_t err = camio_connect(priv->base, &base_channel);
    if(err){
        if(err != CAMIO_ETRYAGAIN){//ETRYAGAIN is ok.
            DBG("EEK unexpected error trying to call connect on base channel\n");
        }
        return err;
    }

    DBG("Making new delimiter channel\n");
    camio_channel_t* channel = NEW_STREAM(delim);
    if(!channel){
        ERR("No memory for new delimter\n");
        *channel_o = NULL;
        return CAMIO_ENOMEM;
    }
    *channel_o = channel;

    DBG("Constructing delimiter delimfn=%p\n", priv->params->delim_fn);
    return delim_channel_construct(channel,this,base_channel, priv->params->delim_fn);
}





/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t delim_construct(camio_controller_t* this, void** params, ch_word params_size)
{
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(delim_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO DELIM : Need better error values
    }
    delim_params_t* delim_params = (delim_params_t*)(*params);

    if(delim_params->delim_fn == NULL){
        DBG("Delimiter function is not populated. Cannot continue!\n");
        return CAMIO_EINVALID; //TODO XXX -- better errors
    }

    //Populate the parameters
    delim_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    priv->params = delim_params;

    //OK try to construct the base channel controller...
    camio_error_t err = camio_device_params_new(
        priv->params->base_uri,
        &priv->base_params,
        &priv->base_params_size,
        &priv->base_id
    );
    if(err){
        DBG("Uh ohh, got %lli error trying to make base controller parameters\n",err);
        return err;
    }

    err = camio_device_constr(priv->base_id,&priv->base_params,priv->base_params_size,&priv->base);
    if(err){
        DBG("Uh ohh, got %lli error trying to construct base controller\n",err);
        return err;
    }


    //Populate the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.controller  = this;
    this->muxable.vtable.ready      = delim_controller_ready;
    this->muxable.fd                = priv->base->muxable.id; //Pass this out, but retain the ready function.
                                                              //Smells a bit bad, but is probably ok.

    DBG("Finished constructing delimiter controller and base controller\n");
    return CAMIO_ENOERROR;
}


static void delim_destroy(camio_controller_t* this)
{
    DBG("Destroying delim controller...\n");
    delim_controller_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->base){ camio_controller_destroy(priv->base); }

    if(priv->params) { free(priv->params); }
    free(this);
    DBG("Done destroying delim controller structure\n");
}

NEW_CONNECTOR_DEFINE(delim, delim_controller_priv_t)
