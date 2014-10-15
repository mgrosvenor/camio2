/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   15 Oct 2014
 *  File name: netmap_if_vec.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_NETMAP_NETMAP_IF_VEC_H_
#define SRC_DRIVERS_NETMAP_NETMAP_IF_VEC_H_

#include "netmap_if.h"
#include "../../../deps/chaste/data_structs/vector/vector_typed_declare_template.h"

declare_ch_vector(netmap_if,netmap_if_t)
declare_ch_vector_cmp(netmap_if,netmap_if_t)

#endif /* SRC_DRIVERS_NETMAP_NETMAP_IF_VEC_H_ */
