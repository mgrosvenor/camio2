/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Oct 13, 2014
 *  File name: stream_state_vec.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include "../types/stdinclude.h"
#include "../../deps/chaste/data_structs/vector/vector_typed_define_template.h"
#include "stream_state_vec.h"


define_ch_vector(CAMIO_STREAM_STATE_VEC,camio_stream_state_t)

//Compare only the key in the key_value pair, as this is the useful thing
//TODO XXX: Port this to using the chaste generic hash map ... when it exists
define_ch_vector_compare(CAMIO_STREAM_STATE_VEC,camio_stream_state_t)
{

    if(lhs->scheme_len < rhs->scheme_len){
        return -1;
    }

    if(lhs->scheme_len > rhs->scheme_len){
        return 1;
    }

    //Key lens are the same now.
    return strncmp(lhs->scheme, rhs->scheme, lhs->scheme_len);
}
