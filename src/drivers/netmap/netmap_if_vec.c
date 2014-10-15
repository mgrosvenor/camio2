/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   15 Oct 2014
 *  File name: netmap_if_vec.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "netmap_if_vec.h"
#include "../../../deps/chaste/data_structs/vector/vector_typed_define_template.h"

define_ch_vector(netmap_if,netmap_if_t);

define_ch_vector_compare(netmap_if,netmap_if_t)
{
    return strcmp(lhs->if_name,rhs->if_name);
}
