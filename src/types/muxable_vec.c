/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 16, 2015
 *  File name:  muxable_vec.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "types.h"
#include <deps/chaste/data_structs/vector/vector_typed_define_template.h>
#include "muxable_vec.h"

#include <src/multiplexers/muxable.h>

define_ch_vector(CAMIO_MUXABLE_VEC,camio_muxable_t)
define_ch_vector_compare(CAMIO_MUXABLE_VEC,camio_muxable_t)
{
    return memcmp(lhs,rhs,sizeof(camio_muxable_t));
}
