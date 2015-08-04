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
#include <assert.h>
#include <deps/chaste/utils/debug.h>

camio_error_t camio_easy_channel_new(char* uri, camio_channel_t** channel)
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

   //Use the parameters structure to construct a new device object
   camio_dev_t* device = NULL;
   err = camio_device_new(id,&params,params_size,&device);
   if(err){
       DBG("Could not construct device\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }
   //DBG("## Got new device at address %p\n", device);

// This code is not really necessary, it's a lot of setup to just spin
//   //Create a new multiplexer object
//   camio_mux_t* mux = NULL;
//   err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE,&mux);
//   if(err){
//       DBG("Could not create multiplexer\n");
//       return CAMIO_EINVALID;
//   }
//
//   err = camio_mux_insert(mux,&device->muxable, 0);
//   if(err){
//       DBG("Could not insert device into multiplexer\n");
//       return CAMIO_EINVALID;
//   }
//
//   camio_muxable_t* muxable = NULL;
//   err = camio_mux_select(mux,&muxable); //Block waiting for device to connect
//   if(err){
//       DBG("Could not select in multiplexer\n");
//       return err;
//   }
//
//   //If not true, we have a problem!
//   assert(muxable->parent.dev == device);
   camio_msg_t msg = {0};
   ch_word len = 1;
   err = camio_ctrl_chan_req(device,&msg,&len);
   if(err){
       DBG("Could not request connection on device\n");
       return err;
   }
   if(len != 1){
       DBG("Could not enqueue request connection on device\n");
       return CAMIO_ETOOMANY;
   }

   while( (err = camio_ctrl_chan_ready(device)) == CAMIO_ETRYAGAIN ){}
   if(err){
       DBG("Could not connect to channel\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }

   err = camio_ctrl_chan_res(device,&msg, &len);
   if(err){
       DBG("Could not get connection response!\n");
       return err;
   }
   if(len != 1){
       DBG("Could not get request connection response\n");
       return CAMIO_ETOOMANY;
   }
   if(msg.ch_res.status){
       DBG("Error getting channel response. Err=%i\n", msg.ch_res.status);
       return msg.ch_res.status;
   }

   *channel = msg.ch_res.channel;
   DBG("## Got new channel at address %p\n", channel);

   //Clean up our mess
   //camio_mux_destroy(mux);
   camio_device_destroy(device);
   return CAMIO_ENOERROR;
}

camio_error_t camio_easy_device_new(char* uri, camio_dev_t** device_o)
{
    ch_word id = -1;
    void* params = NULL;
    ch_word params_size = 0;
    camio_error_t err = camio_device_params_new(uri,&params,&params_size, &id);
    if(err){
        ERR("Invalid device specification %s\n", uri);
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    //Use the parameters structure to construct a new device object
    err = camio_device_new(id,&params,params_size,device_o);
    if(err){
        DBG("Could not construct device\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    return CAMIO_ENOERROR;
}
