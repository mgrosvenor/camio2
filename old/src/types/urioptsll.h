// CamIO 2: urioptsll.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef URIOPTSLL_H_
#define URIOPTSLL_H_

#include "../types/stdinclude.h"
#include "deps/chaste/types/types.h"

typedef enum { URIOPT_FLAG, URIOPT_REQUIRED, URIOPT_OPTIONAL } uri_opt_mode;

typedef struct {
    char* opt_name;
    uri_opt_mode opt_mode;
    ch_types_e opt_type; //Will use the chaste options parser
    bool found;
    void* result;
    uint64_t result_size;
} uri_opt;

#include "../../deps/chaste/data_structs/linked_list/linked_list_typed_declare_template.h"

//Declare a linked list of key_value items
declare_ch_llist(URIOPT,uri_opt)
declare_ch_llist_cmp(URIOPT,uri_opt)


#endif /* URIOPTSLL_H_ */
