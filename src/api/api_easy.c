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

camio_error_t camio_stream_new(char* uri, camio_stream_t** stream)
{
   //Make and populate a parameters structure
   void* params;
   ch_word params_size;
   ch_word id;
   camio_error_t err = camio_transport_params_new(uri,&params, &params_size, &id);
   if(err){
       DBG("Could not convert parameters into structure\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }
   //DBG("Got parameter at %p with size %i and id=%i\n", params, params_size, id);

   //Use the parameters structure to construct a new connector object
   camio_connector_t* connector = NULL;
   err = camio_transport_constr(id,&params,params_size,&connector);
   if(err){
       DBG("Could not construct connector\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }
   //DBG("## Got new connector at address %p\n", connector);

// This code is not really necessary, it's a lot of setup to just spin
//   //Create a new multiplexer object
//   camio_mux_t* mux = NULL;
//   err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE,&mux);
//   if(err){
//       DBG("Could not create multiplexer\n");
//       return CAMIO_EINVALID;
//   }
//
//   err = camio_mux_insert(mux,&connector->muxable, 0);
//   if(err){
//       DBG("Could not insert connector into multiplexer\n");
//       return CAMIO_EINVALID;
//   }
//
//   camio_muxable_t* muxable = NULL;
//   err = camio_mux_select(mux,&muxable); //Block waiting for connector to connect
//   if(err){
//       DBG("Could not select in multiplexer\n");
//       return err;
//   }
//
//   //If not true, we have a problem!
//   assert(muxable->parent.connector == connector);

   while( (err = camio_connect(connector,stream)) == CAMIO_ETRYAGAIN ){}
   if(err){
       DBG("Could not connect to stream\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }


   DBG("## Got new stream at address %p\n", stream);

   //Clean up our mess
   //camio_mux_destroy(mux);
   camio_connector_destroy(connector);
   return CAMIO_ENOERROR;
}

camio_error_t camio_connector_new(char* uri, camio_connector_t** connector_o)
{
    ch_word id = -1;
    void* params = NULL;
    ch_word params_size = 0;
    camio_error_t err = camio_transport_params_new(uri,&params,&params_size, &id);
    if(err){
        ERR("Invalid transport specification %s\n", uri);
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    //Use the parameters structure to construct a new connector object
    err = camio_transport_constr(id,&params,params_size,connector_o);
    if(err){
        DBG("Could not construct connector\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    return CAMIO_ENOERROR;
}
