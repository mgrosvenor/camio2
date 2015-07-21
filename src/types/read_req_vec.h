/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Oct 13, 2014
 *  File name: channel_state_vec.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef SRC_TYPES_CHANNEL_STATE_VEC_H_
#define SRC_TYPES_CHANNEL_STATE_VEC_H_

#include "types.h"
#include <src/devices/controller.h>
#include <src/types/device_params_vec.h>
#include <deps/chaste/data_structs/vector/vector_typed_declare_template.h>
#include <src/utils/uri_parser/uri_parser.h>

declare_ch_vector(CAMIO_RREQ,camio_read_req_t)
declare_ch_vector_cmp(CAMIO_RREQ,camio_read_req_t)


#endif /* SRC_TYPES_CHANNEL_STATE_VEC_H_ */
