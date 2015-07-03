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
#include <deps/chaste/parsing/numeric_parser.h>
#include <deps/chaste/parsing/bool_parser.h>
#include <deps/chaste/utils/util.h>

#include <src/multiplexers/muxable.h>
#include <src/multiplexers/spin_mux.h>


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
    ch_word param_struct_hier_offset,
    camio_construct_f construct,
    ch_word param_struct_size,
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params,
    ch_word global_store_size
)
{

    //DBG("got params=%p\n", params);
    //First check that the scheme hasn't already been registered
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state = __camio_state_container.trans_state;

    camio_transport_state_t tmp = {
        .scheme             = scheme,
        .scheme_len         = scheme_len
    };

    camio_transport_state_t* found = trans_state->find(trans_state,trans_state->first,trans_state->end,tmp);

    if(NULL == found){//Transport has not yet been registered

        camio_transport_state_t state = {
            .scheme                     = scheme,
            .scheme_len                 = scheme_len,
            .param_struct_hier_offset   = param_struct_hier_offset,
            .param_struct_size          = param_struct_size,
            .params                     = params,
            .construct                  = construct,
            .global_store_size          = global_store_size,
            .global_store               = NULL
        };


        //If the transport wants a global store, allocate it
        if(global_store_size > 0){
            state.global_store = calloc(1,global_store_size);
            if(NULL == state.global_store){
                return CAMIO_ENOMEM;
            }
        }

        DBG("Adding new scheme name=%s. Transport count =%i/%i\n", state.scheme, trans_state->count, trans_state->size);
        trans_state->push_back(trans_state,state);

    }
    else{
        return CAMIO_EINVALID;
    }

    return CAMIO_ENOERROR;
}




camio_error_t camio_transport_params_new( ch_cstr uri_str, void** params_o, ch_word* params_size_o, ch_word* id_o )
{
    //DBG("Making new params\n");
    if(!__camio_state_container.is_initialized){
        init_camio();
    }
    camio_error_t result = CAMIO_ENOERROR;

    //Try to parse the URI. Does it make sense?
    camio_uri_t* uri;
    result = parse_uri(uri_str,&uri);
    if(result != CAMIO_ENOERROR){
        goto exit_uri;
    }
    DBG("Parsed URI\n");
    DBG("Got Scheme :%.*s\n", uri->scheme_name_len, uri->scheme_name );

    //Parsing gives us a scheme. Now check that the scheme has been registered and find it if it has
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state = __camio_state_container.trans_state;
    camio_transport_state_t  tmp = { 0 };
    tmp.scheme     = uri->scheme_name;
    tmp.scheme_len = uri->scheme_name_len;
    camio_transport_state_t* state = trans_state->find(trans_state,trans_state->first,trans_state->end, tmp);
    if(NULL == state){//transport has not yet been registered
        result = CAMIO_NOTIMPLEMENTED;
        goto exit_uri;
    }
    DBG("Got state scheme:%.*s\n", state->scheme_len, state->scheme);

    //There is a valid scheme -> transport mapping. Now make a parameters structure and try to populate it
    DBG("making params_struct of size %lli\n", state->param_struct_size );
    char* params_struct = calloc(1, state->param_struct_size);
    void* params_struct_value = &params_struct[state->param_struct_hier_offset];
    len_string_t* param_ptr = (len_string_t*)params_struct_value;
    DBG("hierarch @ %i =%.*s [%i]\n", state->param_struct_hier_offset, uri->hierarchical_len, uri->hierarchical, uri->hierarchical_len);
    strncpy(param_ptr->str, uri->hierarchical, MIN(LSTRING_MAX,uri->hierarchical_len));
    param_ptr->str_len = uri->hierarchical_len;


    //iterate over the parameters list, checking for parameters
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = state->params;
    CH_LIST(KV)* uri_opts = uri->key_vals;
    DBG("Iterating over %i parameters...\n", params->count);
    //DBG("params->first=%p, params->end=%p, uri_opts=%p\n", params->first, params->end, uri_opts);
    for( camio_transport_param_t* param = params->first; param != params->end; param = params->next(params,param) ){

        //Search for the parameter name (ie key) in the key/value uri options list. This should answer the question,
        //"Was the parameter with a given name present in the URI supplied".
        CH_LIST_IT(KV) found = {0};
        if(uri_opts){
            key_val kv = { .key = param->param_name, .key_len = strlen(param->param_name) };
            CH_LIST_IT(KV) first = uri_opts->first(uri_opts);
            CH_LIST_IT(KV) end   = uri_opts->end(uri_opts);
            found = uri_opts->find(uri_opts,&first, &end, kv);
        }

        //We found it! OK. try to num parse it just in case its a number. If not, this will have no effect.
        num_result_t num_result = {0};
        ch_cstr value           = NULL;
        ch_word value_len       = 0;
        if(found.value){
            value = found.value->value;
            value_len = found.value->value_len;
            DBG("param found param=%s value=%.*s offset=%lli. Parsing as a number now.\n", param->param_name, value_len, value, param->param_struct_offset);
            num_result  = parse_nnumber(value, 0, value_len); //Just try to parse this as a number in casenum_result.type
            DBG("num_result.type=%i\n", num_result.type);
        }

        //Some sanity checking...
        if(found.value && param->found){ //Oh shit, we already did this before! Can only assign once!
            DBG("Parameter with name %s already found. You can only assign once!\n", param->param_name);
            result = CAMIO_EINVALID; //TODO XXX make a better return value
            goto exit_params;
        }

        if(!found.value && param->opt_mode == CAMIO_TRANSPORT_PARAMS_MODE_REQUIRED){ //Oh shit this was required!
            DBG("Parameter with name %s is required but not supplied!\n", param->param_name);
            result = CAMIO_EINVALID; //TODO XXX make a better return value
            goto exit_params;
        }

        //Assuming there is either an option int the URI, or a default value, where will we place the result?
        void* params_struct_value = &params_struct[param->param_struct_offset];

        //What is the type of the option that the parameter expects?
        switch(param->type){
            //String are special
            case CAMIO_TRANSPORT_PARAMS_TYPE_LSTRING:{
                len_string_t* param_ptr = (len_string_t*)params_struct_value;
                if(found.value){ //We got a value, let's try to assign it
                    DBG("value=%.*s [%i]\n", value_len, value, value_len);
                    strncpy(param_ptr->str, value, MIN(LSTRING_MAX,value_len));
                    param_ptr->str_len = value_len;
                }
                else{//It must now be true that found.value==NULL && param->mode==CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL
                    len_string_t* param_ptr = (len_string_t*)params_struct_value;
                    DBG("value=%.*s [%i]\n", param->default_val.len_string_t_val.str_len,
                            param->default_val.len_string_t_val.str, param->default_val.len_string_t_val.str_len);
                    *param_ptr = param->default_val.len_string_t_val;
                }
                param->found = true; //Woo hoo!
                break;
            }

            case CAMIO_TRANSPORT_PARAMS_TYPE_UINT64:{
                uint64_t* param_ptr = (uint64_t*)params_struct_value;
                if(found.value){ //We got a value, let's try to assign it
                    if(num_result.type == CH_UINT64) {  *param_ptr = (uint64_t)num_result.val_uint; }
                    else{ //Shit, the value has the wrong type!
                        DBG("Expected a UINT64 but got \"%.*s\" ", value_len, value);
                        result = CAMIO_EINVALID; //TODO XXX make a better return value
                        goto exit_params;
                    }
                }
                else{//It must now be true that found.value==NULL && param->mode==CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL
                    *param_ptr = param->default_val.uint64_t_val;
                }
                param->found = true; //Woo hoo!
                break;
            }

            //Promote UINT64 up to INT64 if necessary
            case CAMIO_TRANSPORT_PARAMS_TYPE_INT64:{
                int64_t* param_ptr = (int64_t*)params_struct_value;
                if(found.value){ //We got a value, let's try to assign it
                    if(      num_result.type == CH_UINT64)  { *param_ptr = (int64_t)num_result.val_uint; }
                    else if( num_result.type == CH_INT64)   { *param_ptr = (int64_t)num_result.val_int;  }
                    else{
                        DBG("Expected an INT64 but got \"%.*s\" ", value_len, value);
                        result = CAMIO_EINVALID; //TODO XXX make a better return value
                        goto exit_params;
                    }
                }
                else{//It must now be true that found.value==NULL && param->mode=CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL
                    *param_ptr = param->default_val.int64_t_val;
                }
                param->found = true; //Woo hoo!
                break;
            }

            //Promote UINT64 and INT64 up to DOUBLE if necessary
            case CAMIO_TRANSPORT_PARAMS_TYPE_DOUBLE:{
                double* param_ptr = (double*)params_struct_value;
                if(found.value){ //We got a value, let's try to assign it
                    if(      num_result.type == CH_UINT64)  { *param_ptr = (double)num_result.val_uint; }
                    else if( num_result.type == CH_INT64)   { *param_ptr = (double)num_result.val_int;  }
                    else if( num_result.type == CH_DOUBLE)  { *param_ptr = (double)num_result.val_dble; }
                    else{
                        DBG("Expected a DOUBLE but got\" %.*s\" ", value_len, value);
                        result = CAMIO_EINVALID; //TODO XXX make a better return value
                        goto exit_params;
                    }
                }
                else{//It must now be true that found.value==NULL && param->mode=CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL
                    *param_ptr = param->default_val.double_val;
                }
                param->found = true; //Woo hoo!
                break;

            }

            default:{
                DBG("EEEK! Unknown type. Did you use the right CAMIO_TRANSPORT_PARAMS_XXX?");
                result = CAMIO_EINVALID; //TODO XXX make a better return value
                goto exit_params;
            }
        }
    }

    DBG("params_o=%p, params_size_o=%p, id_o=%p\n", params_o, params_size_o, id_o);


    //Output the things that we care about
    *params_o       = params_struct;
    *params_size_o  = state->param_struct_size;
    *id_o           = trans_state->get_idx(trans_state,state);

    DBG("Done making new parameters struct!\n");
    return CAMIO_ENOERROR;


exit_params:
    free(params_struct);

exit_uri:
    free_uri(&uri);

    //Stick some "sane" values in here
    *params_o      = NULL;
    *params_size_o = 0;
    *id_o = -1;

    return result;
}



camio_error_t camio_transport_constr(ch_word id, void** params, ch_word params_size, camio_connector_t** connector_o)
{
    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state = __camio_state_container.trans_state;
    camio_transport_state_t* state = trans_state->off(trans_state,id);
    if(state == NULL){
        DBG("Could not find transport at ID=%i\n",id);
        return CAMIO_EINDEXNOTFOUND;
    }

    return state->construct(params,params_size,connector_o);
}



camio_error_t camio_transport_get_id( ch_cstr scheme_name, ch_word* id_o)
{
    DBG("Searching for scheme named %s\n",scheme_name);
    if(!__camio_state_container.is_initialized){
        init_camio();
    }

    CH_VECTOR(CAMIO_TRANSPORT_STATE_VEC)* trans_state = __camio_state_container.trans_state;
    camio_transport_state_t  tmp = { 0 };
    tmp.scheme     = scheme_name;
    tmp.scheme_len = strlen(scheme_name);
    camio_transport_state_t* state = trans_state->find(trans_state,trans_state->first,trans_state->end, tmp);
    if(NULL == state){//transport has not yet been registered
        DBG("Could not find scheme named %s\n", scheme_name);
        return CAMIO_NOTIMPLEMENTED;
    }
    DBG("Got state scheme:%.*s\n", state->scheme_len, state->scheme);

    *id_o = trans_state->get_idx(trans_state,state);
    return CAMIO_ENOERROR;
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



camio_error_t camio_mux_new( camio_mux_hint_e hint, camio_mux_t** mux_o)
{
    camio_mux_t* result = NULL;
    switch(hint){
        case CAMIO_MUX_HINT_PERFORMANCE:{
            result = NEW_MUX(spin);
            if(!result){
                return CAMIO_ENOMEM;
            }
            break;
        }
        default:
            return CAMIO_EINVALID;//TODO XXX better error code
    }

    result->vtable.construct(result);
    *mux_o = result;
    return CAMIO_ENOERROR;
}


