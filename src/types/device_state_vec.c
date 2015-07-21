/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Oct 13, 2014
 *  File name: channel_state_vec.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include "../types/types.h"
#include "../../deps/chaste/data_structs/vector/vector_typed_define_template.h"
#include "device_state_vec.h"

//TODO XXX: Rename this to "device state" to make the naming consistent.

define_ch_vector(CAMIO_TRANSPORT_STATE_VEC,camio_device_state_t)

//Compare only the scheme name and length
//TODO XXX: Port this to using the chaste generic hash map ... when it exists
define_ch_vector_compare(CAMIO_TRANSPORT_STATE_VEC,camio_device_state_t)
{

//    printf("Comparing %.*s [%i] with %.*s [%i]\n",
//            (int)rhs->scheme_len, rhs->scheme,(int) rhs->scheme_len,
//            (int)lhs->scheme_len, lhs->scheme, (int)lhs->scheme_len);

    if(lhs->scheme_len < rhs->scheme_len){
        return -1;
    }

    if(lhs->scheme_len > rhs->scheme_len){
        return 1;
    }



    //Key lens are the same now.
    return strncmp(lhs->scheme, rhs->scheme, lhs->scheme_len);
}
