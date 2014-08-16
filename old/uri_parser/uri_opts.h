// CamIO 2: uri_opts.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef URI_OPTS_H_
#define URI_OPTS_H_

#include "deps/chaste/types/types.h"

#include "../types/stdinclude.h"
#include "../errors/errors.h"

#include "../types/urioptsll.h"
#include "uri_opts.h"
#include "uri_parser.h"

/*
 * The URI parser allows streams to add options (parameters) to their URIs that are required, optional or flags. The URI
 * parser will then do the necessary checking and type conversion to make these parameters useful.
 */

typedef struct {
    CH_LIST(URIOPT)* uri_opts;
} uri_opt_parser;

//Make a new uri parser
int uri_opt_parser_new(uri_opt_parser** opt_praser_o);

//Free a uri parser
void uri_opt_parser_free(uri_opt_parser** opt_parser_o);

//Add a new option to the URI parser and point it to a variable to contain the results
int uri_opt_parser_add(uri_opt_parser* parser, char* name, uri_opt_mode mode, ch_types_e type, void* result);

//Parse the URI. It this fails return an error otherwise return ENOERROR
int uri_opt_parser_parse(uri_opt_parser* opt_parser, uri* parsed_uri);

#endif /* URI_OPTS_H_ */
