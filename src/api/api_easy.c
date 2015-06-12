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

   //Use the connector to connect to a stream assume that the stream connects immidately
   err = camio_connect(connector,stream);
   if(err){
       DBG("Could not connect to stream\n");
       return CAMIO_EINVALID; //TODO XXX put a better error here
   }
   //DBG("## Got new stream at address %p\n", stream);

   camio_connector_destroy(connector); //Clean up our mess

   return CAMIO_ENOERROR;
}
