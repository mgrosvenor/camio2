// CamIO 2: uri_opts.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef URI_OPTS_H_
#define URI_OPTS_H_

#include "deps/chaste/types/types.h"

#include "../types/stdinclude.h"
#include "../errors/errors.h"

#include "../types/urioptsll.h"

/*
 * typedef enum { URIOPT_FLAG, URIOPT_REQUIRED, URIOPT_OPTIONAL } uri_opt_mode;

typedef struct {
    char* opt_name;
    uri_opt_mode opt_mode;
    ch_types_e opt_type; //Will use the chaste options parser
    bool found;
    void* result;
} uri_opt;
 *
 */


typedef struct {
    CH_LIST(URIOPT)* uri_opts;
} uri_opt_parser;


int uri_opt_parser_new(uri_opt_parser** opt_praser_o)
{
    uri_opt_parser* result = calloc(1,sizeof(uri_opt_parser));
    if(!result){
        return CIO_ENOMEM;
    }

    result->uri_opts = CH_LIST_NEW(URIOPT,NULL);
    if(!result->uri_opts){
        return CIO_ENOMEM;
    }


    *opt_praser_o = result;
    return CIO_ENOERROR;
}


void uri_opt_parser_free(uri_opt_parser** opt_parser_o)
{
    if(!opt_parser_o){
        return;
    }

    if(!*opt_parser_o){
        return;
    }

    if(*opt_parser_o->uri_opts){
        (*opt_parser_o)->uri_opts->delete(*opt_parser_o->uri_opts);
    }

    free(*opt_parser_o);

    *opt_parser_o = NULL;
}


int uri_opt_parser_add(uri_opt_parser* parser, char* name, uri_opt_mode mode, ch_types_e type, void* result)
{
    uri_opt tmp = { .opt_name = name, .opt_mode = mode, .opt_type = type, .found = false, .result = result };

    //Check that the name is not in the list
    CH_LIST_IT(URIOPT)* it = parser->uri_opts->find(parser->uri_opts, tmp);
    if(it->value){
        return CIO_EURIOPTEXITS;
    }

    parser->uri_opts->push_back(parser->uri_opts,tmp);
    return CIO_ENOERROR;
}


#endif /* URI_OPTS_H_ */
