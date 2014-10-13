// CamIO 2: uri_opts.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 


//This is a nice idea, but it's not written yet, todo!


//#include "uri_opts.h"
//#include "deps/chaste/types/types.h"
//#include "deps/chaste/parsing/numeric_parser.h"
//#include "deps/chaste/parsing/bool_parser.h"
//
//
////int uri_opt_parser_new(uri_opt_parser** opt_praser_o)
//{
//    uri_opt_parser* result = calloc(1,sizeof(uri_opt_parser));
//    if(!result){
//        return CAMIO_ENOMEM;
//    }
//
//    result->uri_opts = CH_LIST_NEW(URIOPT,CH_LIST_CMP(URIOPT));
//    if(!result->uri_opts){
//        return CAMIO_ENOMEM;
//    }
//
//
//    *opt_praser_o = result;
//    return CAMIO_ENOERROR;
//}
//
//
//void uri_opt_parser_free(uri_opt_parser** opt_parser_o)
//{
//    if(!opt_parser_o){
//        return;
//    }
//
//    if(!*opt_parser_o){
//        return;
//    }
//
//    if((*opt_parser_o)->uri_opts){
//        (*opt_parser_o)->uri_opts->delete( (*opt_parser_o)->uri_opts);
//    }
//
//    free(*opt_parser_o);
//
//    *opt_parser_o = NULL;
//}
//
//
//int uri_opt_parser_add(uri_opt_parser* parser, char* name, uri_opt_mode mode, ch_types_e type, void* result)
//{
//    uri_opt tmp = { .opt_name = name, .opt_mode = mode, .opt_type = type, .found = false, .result = result };
//
//    //Check that the name is not in the list
//    CH_LIST_IT(URIOPT) start = parser->uri_opts->first(parser->uri_opts);
//    CH_LIST_IT(URIOPT) end   = parser->uri_opts->end(parser->uri_opts);
//    CH_LIST_IT(URIOPT) it    = parser->uri_opts->find(parser->uri_opts, &start, &end, tmp);
//    if(it.value){
//        return CAMIO_EURIOPTEXITS;
//    }
//
//    parser->uri_opts->push_back(parser->uri_opts,tmp);
//    return CAMIO_ENOERROR;
//}
//
//int uri_opt_parser_parse(uri_opt_parser* opt_parser, camio_uri_t* parsed_uri)
//{
//    for(CH_LIST_IT(URIOPT)it = opt_parser->uri_opts->first(opt_parser->uri_opts); it.value;
//            opt_parser->uri_opts->next(opt_parser->uri_opts,&it)){
//
//        //XXX HACK! - Define a key_val structure with the name we want so that we can search by name. Really should use a
//        //            dictionary for this sort of thing.
//        key_val tmp = { .key = it.value->opt_name, .key_len = strlen(it.value->opt_name), .value = NULL, .value_len = 0 };
//        CH_LIST_IT(KV) start = parsed_uri->key_vals->first(parsed_uri->key_vals);
//        CH_LIST_IT(KV) end = parsed_uri->key_vals->end(parsed_uri->key_vals);
//        CH_LIST_IT(KV)found = parsed_uri->key_vals->find(parsed_uri->key_vals, &start, &end, tmp);
//        if(!found.value ){
//            //Not found, and we care about it
//            if(it.value->opt_mode == URIOPT_REQUIRED){
//                return CHI_EURIOPTREQUIRED;
//            }
//
//            //Not found, but we don't care
//            continue;
//        }
//
//        //We found a key without a value...
//        if( NULL == found.value->value || 0 == found.value->key_len  ){
//
//            //and it's not a flag
//            if(( URIOPT_FLAG != it.value->opt_mode)){
//                return CHI_EURIOPTNOTFLAG;
//            }
//
//            *(bool*)it.value->result = true;
//            it.value->result_size  = sizeof(ch_bool);
//            continue; //All done, flag sorted out
//        }
//
//        //If we expect a numeric, try using the numeric parser
//        if(it.value->opt_type == CH_UINT64 ||
//           it.value->opt_type == CH_INT64  ||
//           it.value->opt_type == CH_DOUBLE ){
//             num_result_t num = parse_number(found.value->value, found.value->value_len);
//
//             //failure to parse this type
//             if(num.type == CH_NO_TYPE){
//                 return CAMIO_EURIOPTBADTYPE;
//             }
//
//             //Otherwise assign it
//             memcpy(it.value->result,&num.val_uint, sizeof(num.val_uint));
//             it.value->result_size  = sizeof(num.val_uint);
//             continue; // All done :-)
//        }
//
//        //If we expect a bool, try using the bool parser
//        if(it.value->opt_type == CH_BOOL){
//            num_result_t num = parse_bool(found.value->value, found.value->value_len, 0);
//
//            if(num.type == CH_NO_TYPE){
//                return CAMIO_EURIOPTBADTYPE;
//            }
//
//            //Otherwise assign it
//            memcpy(it.value->result,&num.val_uint, sizeof(num.val_uint));
//            it.value->result_size  = sizeof(num.val_uint);
//            continue; // All done :-)
//        }
//
//
//        //If we expect a string, try just assigning the string
//        if(it.value->opt_type == CH_STRING){
//            *(char**)it.value->result      = found.value->value;
//            it.value->result_size = found.value->key_len;
//        }
//
//
//        //We have run out of options, no idea what we're looking for now
//        return CAMIO_EURIOPTUNKNOWNTYPE;
//
//    }
//    return CAMIO_ENOERROR;
//}
