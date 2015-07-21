/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 12, 2015
 *  File name:  api_easy.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#include "api_easy.h"
#include <src/camio_debug.h>
#include <assert.h>

camio_error_t camio_channel_new(char* uri, camio_channel_t** channel)
{
   //Make and populate a parameters structure
   void* params;
   ch_word params_size;
   ch_word id;
   camio_error_t err = camio_device_params_new(uri,&params, &params_size, &id);
   if(err){
       DBG("Could not convert parameters into structure\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }
   //DBG("Got parameter at %p with size %i and id=%i\n", params, params_size, id);

   //Use the parameters structure to construct a new controller object
   camio_controller_t* controller = NULL;
   err = camio_device_constr(id,&params,params_size,&controller);
   if(err){
       DBG("Could not construct controller\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }
   //DBG("## Got new controller at address %p\n", controller);

// This code is not really necessary, it's a lot of setup to just spin
//   //Create a new multiplexer object
//   camio_mux_t* mux = NULL;
//   err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE,&mux);
//   if(err){
//       DBG("Could not create multiplexer\n");
//       return CAMIO_EINVALID;
//   }
//
//   err = camio_mux_insert(mux,&controller->muxable, 0);
//   if(err){
//       DBG("Could not insert controller into multiplexer\n");
//       return CAMIO_EINVALID;
//   }
//
//   camio_muxable_t* muxable = NULL;
//   err = camio_mux_select(mux,&muxable); //Block waiting for controller to connect
//   if(err){
//       DBG("Could not select in multiplexer\n");
//       return err;
//   }
//
//   //If not true, we have a problem!
//   assert(muxable->parent.controller == controller);
   camio_chan_req_t req = {0};
   err = camio_ctrl_chan_req(controller,&req,1,0);
   if(err){
       DBG("Could not request connection on controller\n");
       return err;
   }

   while( (err = camio_ctrl_chan_ready(controller)) == CAMIO_ETRYAGAIN ){}
   if(err){
       DBG("Could not connect to channel\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }

   camio_chan_req_t* res = NULL;
   err = camio_ctrl_chan_res(controller,&res);
   if(err){
       DBG("Could not get connection response!\n");
       return err;
   }

   *channel = res->channel;
   DBG("## Got new channel at address %p\n", channel);

   //Clean up our mess
   //camio_mux_destroy(mux);
   camio_controller_destroy(controller);
   return CAMIO_ENOERROR;
}

camio_error_t camio_controller_new(char* uri, camio_controller_t** controller_o)
{
    ch_word id = -1;
    void* params = NULL;
    ch_word params_size = 0;
    camio_error_t err = camio_device_params_new(uri,&params,&params_size, &id);
    if(err){
        ERR("Invalid device specification %s\n", uri);
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    //Use the parameters structure to construct a new controller object
    err = camio_device_constr(id,&params,params_size,controller_o);
    if(err){
        DBG("Could not construct controller\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    return CAMIO_ENOERROR;
}
