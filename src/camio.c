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
#include <src/types/transport_state_vec.h>
#include <src/types/transport_params_vec.h>
#include <src/utils/uri_parser/uri_parser.h>
#include <src/errors/camio_errors.h>




camio_t* init_camio()
{
    DBG("Initializing CamIO 2.0...\n");

    //Set up the transport state list, this could be a hashmap one day....
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state =
        CH_VECTOR_NEW(CAMIO_TRANSPORT_STATE_VEC,1024,CH_VECTOR_CMP(CAMIO_TRANSPORT_STATE_VEC));

    __camio_state_container.trans_state = trans_state;

    camio_init_all_transports();

    __camio_state_container.is_initialized = true;

    DBG("Initializing CamIO 2.0...Done\n");
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
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params,
    ch_word global_store_size
)
{

    DBG("got params=%p\n", params);
    //First check that the scheme hasn't already been registered
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state = __camio_state_container.trans_state;

    camio_transport_state_t tmp = {
        .scheme             = scheme,
        .scheme_len         = scheme_len
    };

    camio_transport_state_t* found = trans_state->find(trans_state,trans_state->first,trans_state->end,tmp);

    if(NULL == found){//Transport has not yet been registered

        camio_transport_state_t state = {
            .scheme             = scheme,
            .scheme_len         = scheme_len,
            .hierarchical       = hierarchical,
            .param_struct_size  = param_struct_size,
            .params             = params,
            .construct          = construct,
            .global_store_size  = global_store_size,
            .global_store       = NULL
        };


        //If the transport wants a global store, allocate it
        if(global_store_size > 0){
            state.global_store = calloc(1,global_store_size);
            if(NULL == state.global_store){
                return CAMIO_ENOMEM;
            }
        }

        DBG("Pushing transport state back. Params=%p\n", state.params);
        trans_state->push_back(trans_state,state);

    }
    else{
        return CAMIO_EINVALID;
    }

    return CAMIO_ENOERROR;
}






camio_error_t camio_transport_params_new( ch_cstr uri_str, void** params_o, ch_word* params_size_o, ch_word* id_o )
{
    DBG("Making new params\n");
    if(!__camio_state_container.is_initialized){
        init_camio();
    }

    //Try to parse the URI. Does it make sense?
    camio_uri_t* uri;
    camio_error_t err = parse_uri(uri_str,&uri);
    if(err){ return err; }
    DBG("Parsed URI\n");
    DBG("Got Scheme :%.*s\n", uri->scheme_name_len, uri->scheme_name );

    //Parsing gives us a scheme. Now check that the scheme has been registered and find it if it has
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state = __camio_state_container.trans_state;
    camio_transport_state_t  tmp = { 0 };
    tmp.scheme     = uri->scheme_name;
    tmp.scheme_len = uri->scheme_name_len;
    camio_transport_state_t* state = trans_state->find(trans_state,trans_state->first,trans_state->end, tmp);
    if(NULL == state){//transport has not yet been registered
        return CAMIO_NOTIMPLEMENTED;
    }
    DBG("Got state scheme:%.*s\n", state->scheme_len, state->scheme);

    //There is a valid scheme -> transport mapping. Now make a parameters structure and try to populate it
    char* params_struct = calloc(1, state->param_struct_size);


    //iterate over the parameters list
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = state->params;
    CH_LIST(KV)* uri_opts = uri->key_vals;
    DBG("Iterating over %i parameters...\n", params->count);
    for( camio_transport_param_t* param = params->first;
         param != params->end;
         param = params->next(params,param) )
    {
        key_val kv = {
           .key = param->param_name,
           .key_len = strlen(param->param_name)
        };

        CH_LIST_IT(KV) first = uri_opts->first(uri_opts);
        CH_LIST_IT(KV) end   = uri_opts->end(uri_opts);
        CH_LIST_IT(KV) found = uri_opts->find(uri_opts,&first, &end, kv);
        if(found.value){
            DBG("PARAM=%s VALUE=%.*s OFFSET=%lli\n", param->param_name, found.value->value_len, found.value->value, param->param_struct_offset);
        }
        else{
            DBG("PARAM=%s NOT FOUND!\n", param->param_name);
            return CAMIO_EINVALID;
        }
    }


    //Output the things that we care about
    *params_o      = params_struct;
    *params_size_o = state->param_struct_size;
    *id_o = trans_state->get_idx(trans_state,state);
//    //TODO XXX:Should check the features here!!
//    if(features){
//        return CAMIO_NOTIMPLEMENTED;
//    }

    free_uri(&uri); //Don't forget to do this somewhere


    return CAMIO_ENOERROR;
}



camio_error_t camio_transport_constr(ch_word id, void** params, ch_word params_size, camio_connector_t** connector_o)
{
    (void)id;
    (void)params;
    (void)params_size;
    (void)connector_o;
    return 0;
}



camio_error_t camio_transport_get_id( ch_cstr scheme_name, ch_word* id_o)
{
    (void)scheme_name;
    (void)id_o;
    return 0;
}


camio_error_t camio_transport_get_global(ch_ccstr scheme, void** global_store)
{

    //Find the transport
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state = __camio_state_container.trans_state;

    camio_transport_state_t state = {
        .scheme             = scheme,
        .scheme_len         = strlen(scheme),
        .construct          = NULL,
        .global_store_size  = 0,
        .global_store       = NULL
    };


    camio_transport_state_t* found = trans_state->find(trans_state,trans_state->first,trans_state->end,state);

    if(NULL == found){//transport has not yet been registered
        return CAMIO_EINDEXNOTFOUND;
    }

    *global_store = found->global_store;
    return CAMIO_ENOERROR;


}



