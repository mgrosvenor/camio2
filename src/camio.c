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
#include <src/errors/camio_errors.h>




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




/**
 * Streams call this function to add themselves into the CamIO string parsing system and to register desires for global
 * resources. The register transport function will assign a unique ID to a stream type that can be used for fast access to
 * it in the future using the binary only interface.
 * TODO XXX: This function should probably be split so that string based interface is not necessary.
 */
camio_error_t register_new_transport(
    ch_ccstr scheme,
    ch_word scheme_len,
    ch_cstr* hierarchical,
    camio_construct_f construct,
    ch_word param_struct_size,
    CH_VECTOR(CAMIO_TRANSPORT_OPT_VEC)* params,
    ch_word global_store_size
)
{

    (void)hierarchical;
    (void)param_struct_size;
    (void)params;

    //First check that the scheme hasn't already been registered
    CH_VECTOR(CAMIO_STREAM_STATE_VEC)* stream_state = __camio_state_container.stream_state;

    camio_transport_state_t state = {
        .scheme             = scheme,
        .scheme_len         = scheme_len,
        .construct          = construct,
        .global_store_size  = global_store_size,
        .global_store       = NULL
    };

    camio_transport_state_t* found = stream_state->find(stream_state,stream_state->first,stream_state->end,state);

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
    camio_transport_state_t** found)
{
    if(!__camio_state_container.is_initialized){
        init_camio();
    }


    //Now check that the scheme has been registered
    CH_VECTOR(CAMIO_STREAM_STATE_VEC)* stream_state = __camio_state_container.stream_state;

    camio_transport_state_t state = {
        .scheme             = scheme,
        .scheme_len         = scheme_len,
        .construct          = NULL,
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

    camio_transport_state_t state = {
        .scheme             = scheme,
        .scheme_len         = strlen(scheme),
        .construct          = NULL,
        .global_store_size  = 0,
        .global_store       = NULL
    };


    camio_transport_state_t* found = stream_state->find(stream_state,stream_state->first,stream_state->end,state);

    if(NULL == found){//stream has not yet been registered
        return CAMIO_EINDEXNOTFOUND;
    }

    *global_store = found->global_store;
    return CAMIO_ENOERROR;


}



camio_error_t camio_transport_params_new( ch_cstr uri, void** params_o, ch_word* params_size_o, ch_word* id_o )
{
    (void)uri;
    (void)params_o;
    (void)params_size_o;
    (void)id_o;
    return 0;
}

camio_error_t camio_transport_constr(ch_word id, void** params, ch_word params_size, camio_connector_t** connector_o)
{
    (void)id;
    (void)params;
    (void)params_size;
    (void)connector_o;
    return 0;
}


