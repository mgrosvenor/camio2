// CamIO 2: urioptsll.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#include "urioptsll.h"

#include "../types/stdinclude.h"
#include "../../deps/chaste/data_structs/linked_list/linked_list_typed_define_template.h"


define_ch_llist(URIOPT,uri_opt)

//Compare only the opt_name in the uri_opt structure, as this is the useful thing
//TODO XXX: Port this to using the chaste generic hash map or dictionary... when it exits
define_ch_llist_compare(URIOPT,uri_opt)
{

    if(strlen(lhs->opt_name) < strlen(rhs->opt_name)){
        return -1;
    }

    if( strlen(lhs->opt_name) > strlen(rhs->opt_name)){
        return 1;
    }

    //opt_name lengths are the same now.
    return strncmp(lhs->opt_name, rhs->opt_name, strlen(lhs->opt_name));
}
