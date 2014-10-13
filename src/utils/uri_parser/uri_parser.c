// CamIO 2: uri_parser.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

/*
 * CamIO URIs have the following general format
 * [<scheme name> ][: <hierarchical part>] [ ? <key> [= <value> ] [? | ,] ] [ # <fragment> ]
 *
 * This parser is a bit simple, it only knows how to find queries that match this pattern exactly.
 *
 * TODO XXX: Some queries may like to use "escaping" and "\"escaping\"" and "\\\"escaping\\\"" (etc) especially
 * in the hierarchical part. This is not yet supported.
 */


#include "uri_parser.h"
#include "../../types/types.h"
#include "../../errors/errors.h"
#include "../../types/kvll.h"



typedef enum { PARSE_SCHEME, PARSE_HIER, PARSE_FRAG, PARSE_KEY_INIT, PARSE_KEY, PARSE_VALUE} uri_parser_states;
int parse_uri(char* uri_str, camio_uri_t** result_o)
{

    camio_uri_t* result = (camio_uri_t*)calloc(1,sizeof(camio_uri_t));
    if(! result){
        return CAMIO_ENOMEM;
    }

    //State machine for parsing URIs
    uri_parser_states state = PARSE_SCHEME;


    for(; *uri_str != '\0'; ++uri_str){
        char c = *uri_str;
        switch(state){
            case PARSE_SCHEME:{
                //Chomp of whitespace at the head
                if(c == ' ' || c == '\t' || c == '\r'){
                    continue;
                }

                //Early exit this state
                if(c == ':'){ state = PARSE_HIER; continue;}
                if(c == '?'){ state = PARSE_KEY_INIT; continue;}
                if(c == '#'){ state = PARSE_FRAG; continue;}

                if(result->scheme_name_len == 0){
                    result->scheme_name = uri_str;
                    result->scheme_name_len = 1;
                    continue;
                }

                ++result->scheme_name_len;
                continue;


            }
            case PARSE_HIER:{

                //Early exit this state
                if(c == '?'){ state = PARSE_KEY_INIT; continue;}
                if(c == '#'){ state = PARSE_FRAG; continue;}

                if( result->hierarchical_len == 0){
                    result->hierarchical = uri_str;
                    result->hierarchical_len = 1;
                    continue;
                }

                ++result->hierarchical_len;
                continue;

            }
            case PARSE_KEY_INIT:{
                //Chomp of whitespace at the head
                if(c == ' ' || c == '\t' || c == '\r'){
                    continue;
                }

                //Early exit this state into the fragment state
                if(c == '#'){ state = PARSE_FRAG; continue;}

                if(!result->key_vals){
                    //Make a new linked list for the key value store. This must be the first key to be parsed
                    result->key_vals = CH_LIST_NEW(KV,CH_LIST_CMP(KV));
                }

                //Add a single empty item into the list, then retrieve it.
                //WARNING: "->value" here refers to the value in the linked list iterator, not the value in the key_value
                //store!
                key_val temp = {0};
                result->key_vals->push_back(result->key_vals, temp);
                key_val* kv = result->key_vals->last(result->key_vals).value;

                if(c == '='){
                    //The key length is empty, and we've found an equals, must be a syntax error
                    return CAMIO_ENOKEY;
                }

                state = PARSE_KEY;
                kv->key = uri_str;
                kv->key_len = 1;

                continue;
            }
            case PARSE_KEY:{
                if(c == '='){ state = PARSE_VALUE; continue; }
                if(c == '?'){ state = PARSE_KEY_INIT; continue;}
                if(c == ','){ state = PARSE_KEY_INIT; continue;}
                if(c == '#'){ state = PARSE_FRAG; continue;}

                key_val* kv = result->key_vals->last(result->key_vals).value;
                kv->key_len++;
                continue;
            }
            case PARSE_VALUE:{
                //Early exit this state
                if(c == '?'){ state = PARSE_KEY_INIT; continue;}
                if(c == ','){ state = PARSE_KEY_INIT; continue;}
                if(c == '#'){ state = PARSE_FRAG; continue;}

                key_val* kv = result->key_vals->last(result->key_vals).value;
                if(kv->value_len == 0){
                    kv->value = uri_str;
                    kv->value_len = 1;
                    continue;
                }

                kv->value_len++;
                continue;
            }
            case PARSE_FRAG:{
                if(result->fragement_len == 0){
                    result->fragement = uri_str;
                    result->fragement_len = 1;
                    continue;
                }

                result->fragement_len++;
                continue;
            }
            default:{
                return CAMIO_EUNKNOWNSTATE;
                break;
            }
        }
    }

    *result_o = result;
    return CAMIO_ENOERROR;
//
}


void free_uri(camio_uri_t** u)
{
    //Free up the linked list
    if( (*u)->key_vals ){
        (*u)->key_vals->delete((*u)->key_vals);
    }

    //Free up the memory associated with this item
    free(*u);

    //Remove any dangline pointers
    *u = NULL;
}
