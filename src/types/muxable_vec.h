/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 16, 2015
 *  File name:  muxable_vec.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef MUXABLE_VEC_H_
#define MUXABLE_VEC_H_

#include <src/types/device_params_vec.h>
#include <deps/chaste/data_structs/vector/vector_typed_declare_template.h>

declare_ch_vector(CAMIO_MUXABLE_VEC,camio_muxable_t)
declare_ch_vector_cmp(CAMIO_MUXABLE_VEC,camio_muxable_t)

#endif /* MUXABLE_VEC_H_ */
