/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Oct 13, 2014
 *  File name: stream_state_vec.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef SRC_TYPES_STREAM_STATE_VEC_H_
#define SRC_TYPES_STREAM_STATE_VEC_H_

#include "types.h"
#include <src/transports/connector.h>
#include <src/types/transport_params_vec.h>
#include <deps/chaste/data_structs/vector/vector_typed_declare_template.h>
#include <src/utils/uri_parser/uri_parser.h>

typedef camio_error_t (*camio_construct_f)(void** params, ch_word params_size, camio_connector_t** connector_o);

typedef struct camio_transport_state_s {
    ch_ccstr scheme;                                // The short name for this stream eg: "tcp" or "udp"
    ch_word scheme_len;

    camio_construct_f construct;                    // Construct the transport from the parameters given

    ch_word param_struct_hier_offset;               //Where should the hierarchical part be kept in the param struct

    ch_word param_struct_size;                      //How big is the transport's parameters structure
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params;  //Details of each parameter that the transport (optionally) takes

    void* global_store;                             // Pointer to a global store for all instances to share state
    ch_word global_store_size;                      // Size of the global store

} camio_transport_state_t;


declare_ch_vector(CAMIO_TRANSPORT_STATE_VEC,camio_transport_state_t)
declare_ch_vector_cmp(CAMIO_TRANSPORT_STATE_VEC,camio_transport_state_t)


#endif /* SRC_TYPES_STREAM_STATE_VEC_H_ */
