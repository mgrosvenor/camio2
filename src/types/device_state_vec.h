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
#include <src/devices/device.h>
#include <src/types/device_params_vec.h>
#include <deps/chaste/data_structs/vector/vector_typed_declare_template.h>
#include <src/utils/uri_parser/uri_parser.h>

typedef camio_error_t (*camio_new_dev_f)(void** params, ch_word params_size, camio_dev_t** device_o);

typedef struct camio_device_state_s {
    ch_ccstr scheme;                                // The short name for this channel eg: "tcp" or "udp"
    ch_word scheme_len;

    camio_new_dev_f new_dev;                        // Construct the device from the parameters given

    ch_word param_struct_hier_offset;               //Where should the hierarchical part be kept in the param struct

    ch_word param_struct_size;                      //How big is the device's parameters structure
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params;  //Details of each parameter that the device (optionally) takes

    void* global_store;                             // Pointer to a global store for all instances to share state
    ch_word global_store_size;                      // Size of the global store

} camio_device_state_t;


declare_ch_vector(CAMIO_TRANSPORT_STATE_VEC,camio_device_state_t)
declare_ch_vector_cmp(CAMIO_TRANSPORT_STATE_VEC,camio_device_state_t)


#endif /* SRC_TYPES_CHANNEL_STATE_VEC_H_ */
