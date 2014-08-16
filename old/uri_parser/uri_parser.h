// CamIO 2: uri_parser.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef URI_PARSER_H_
#define URI_PARSER_H_

#include "../types/kvll.h"

//<scheme name> :["] <hierarchical part> ["][ ? <query> ] [ # <fragment> ]
typedef struct{
    char* scheme_name;
    uint64_t scheme_name_len;

    char* hierarchical;
    uint64_t hierarchical_len;

    CH_LIST(KV)* key_vals;

    char* fragement;
    uint64_t fragement_len;

} uri;

int parse_uri(char* uri_str, uri** result);
void free_uri(uri** u);




#endif /* URI_PARSER_H_ */
