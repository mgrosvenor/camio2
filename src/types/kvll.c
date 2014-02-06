// CamIO 2: kvll.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#include "../types/stdinclude.h"
#include "../../deps/chaste/data_structs/linked_list/linked_list_typed_define_template.h"
#include "kvll.h"


define_ch_llist(KV,key_val)

//Compare only the key in the key_value pair, as this is the useful thing
//TODO XXX: Port this to using the chaste generic hash map ... when it exits
define_ch_llist_compare(KV,key_val)
{

    if(lhs->key_len < rhs->key_len){
        return -1;
    }

    if(lhs->key_len > rhs->key_len){
        return 0;
    }

    //Key lens are the same now.
    return strncmp(lhs->key, rhs->key, lhs->key_len);
}
