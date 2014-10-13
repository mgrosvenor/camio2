// CamIO 2: uri_parser.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef URI_PARSER_H_
#define URI_PARSER_H_

#include "../../types/kvll.h"

//<scheme name> :["] <hierarchical part> ["][ ? <query> ] [ # <fragment> ]
typedef struct camio_uri_s{
    ch_cstr scheme_name;
    uint64_t scheme_name_len;

    ch_cstr hierarchical;
    uint64_t hierarchical_len;

    CH_LIST(KV)* key_vals;

    ch_cstr  fragement;
    uint64_t fragement_len;

} camio_uri_t;

int parse_uri(ch_cstr uri_str, camio_uri_t** result);
void free_uri(camio_uri_t** u);




#endif /* URI_PARSER_H_ */
