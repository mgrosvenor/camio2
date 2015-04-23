/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: camio.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stdio.h>
#include <stdarg.h>

#include "camio.h"
#include "camio_debug.h"
#include "camio_init_all.h"
#include <src/types/stream_state_vec.h>
#include <src/utils/uri_parser/uri_parser.h>





camio_t* init_camio()
{
    DBG("Initializing CamIO 2.0...\n");

    //Set up the stream state list, this should really be a hashmap one day....
    CH_VECTOR(CAMIO_STREAM_STATE_VEC)* stream_state =
        CH_VECTOR_NEW(CAMIO_STREAM_STATE_VEC,1024,CH_VECTOR_CMP(CAMIO_STREAM_STATE_VEC));

    __camio_state_container.stream_state = stream_state;

    camio_init_all_transports();

    __camio_state_container.is_initialized = true;
    return &__camio_state_container;
}





camio_error_t register_new_transport(ch_ccstr scheme, ch_word scheme_len, camio_construct_str_f construct_str,
    camio_construct_bin_f construct_bin, ch_word global_store_size )
{
    //First check that the scheme hasn't already been registered
    CH_VECTOR(CAMIO_STREAM_STATE_VEC)* stream_state = __camio_state_container.stream_state;

    camio_stream_state_t state = {
        .scheme             = scheme,
        .scheme_len         = scheme_len,
        .construct_str      = construct_str,
        .construct_bin      = construct_bin,
        .global_store_size  = global_store_size,
        .global_store       = NULL
    };

    camio_stream_state_t* found = stream_state->find(stream_state,stream_state->first,stream_state->end,state);

    if(NULL == found){//Transport has not yet been registered

        //If the transport wants a global store, allocate it
        if(global_store_size > 0){
            state.global_store = calloc(1,global_store_size);
            if(NULL == state.global_store){
                return CAMIO_ENOMEM;
            }
        }

        stream_state->push_back(stream_state,state);
    }

    return CAMIO_ENOERROR;
}





camio_error_t new_camio_transport_generic(ch_ccstr scheme, ch_word scheme_len, camio_transport_features_t* features,
    camio_stream_state_t** found)
{
    if(!__camio_state_container.is_initialized){
        init_camio();
    }


    //Now check that the scheme hasn't already been registered
    CH_VECTOR(CAMIO_STREAM_STATE_VEC)* stream_state = __camio_state_container.stream_state;

    camio_stream_state_t state = {
        .scheme             = scheme,
        .scheme_len         = scheme_len,
        .construct_str      = NULL,
        .construct_bin      = NULL,
        .global_store_size  = 0,
        .global_store       = NULL
    };

    *found = stream_state->find(stream_state,stream_state->first,stream_state->end,state);

    if(NULL == found){//stream has not yet been registered
        return CAMIO_NOTIMPLEMENTED;
    }

    //TODO XXX:Should check the features here!!
    if(features){
        return CAMIO_NOTIMPLEMENTED;
    }

    return CAMIO_ENOERROR;


}


camio_error_t camio_transport_get_global(ch_ccstr scheme, void** global_store)
{

    //Find the stream
    CH_VECTOR(CAMIO_STREAM_STATE_VEC)* stream_state = __camio_state_container.stream_state;

    camio_stream_state_t state = {
        .scheme             = scheme,
        .scheme_len         = strlen(scheme),
        .construct_str      = NULL,
        .construct_bin      = NULL,
        .global_store_size  = 0,
        .global_store       = NULL
    };


    camio_stream_state_t* found = stream_state->find(stream_state,stream_state->first,stream_state->end,state);

    if(NULL == found){//stream has not yet been registered
        return CAMIO_EINDEXNOTFOUND;
    }

    *global_store = found->global_store;
    return CAMIO_ENOERROR;


}





camio_error_t new_camio_transport_str( ch_cstr uri, camio_transport_features_t* features, camio_connector_t** connector_o)
{

    //Try to parse the URI. Does it make sense?
    camio_uri_t* uri_o;
    camio_error_t err = parse_uri(uri,&uri_o);
    if(err){
        return err;
    }

    camio_stream_state_t* found;
    camio_error_t result = new_camio_transport_generic(uri_o->scheme_name, uri_o->scheme_name_len, features, &found);
    if(result){
        return result;
    }

    result = found->construct_str(uri_o,connector_o);
    return result;

}





camio_error_t new_camio_transport_bin(ch_ccstr scheme, camio_transport_features_t* features,
    camio_connector_t** connector_o, ...)
{

    camio_stream_state_t* found;
    camio_error_t result = new_camio_transport_generic(scheme, strlen(scheme),  features, &found);
    if(result){
        return result;
    }


    va_list args;
    va_start(args, connector_o);
    result = found->construct_bin(connector_o,args);
    va_end(args);

    return result;


}



