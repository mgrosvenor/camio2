// CamIO 2: urill.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef KVLL_H_
#define KVLL_H_

#include "stdinclude.h"

typedef struct key_val_s {
    char* key;
    uint64_t key_len;

    char* value;
    uint64_t value_len;
} key_val;


#include "../../deps/chaste/data_structs/linked_list/linked_list_typed_declare_template.h"

//Declare a linked list of key_value items
declare_ch_llist(KV,key_val)
declare_ch_llist_cmp(KV,key_val)


#endif /* KVILL_H_ */
