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


#include <src/transports/connector.h>
#include "types.h"
#include "../../deps/chaste/data_structs/vector/vector_typed_declare_template.h"
#include "../utils/uri_parser/uri_parser.h"

typedef camio_error_t (*camio_construct_f)(void** params, ch_word params_size, camio_connector_t** connector_o);

typedef struct camio_stream_state_s {
    ch_ccstr scheme;                            // The short name for this stream eg: "tcp" or "udp"
    ch_word scheme_len;

    camio_construct_f construct;                // Construct from a binary

    void* global_store;                         // Pointer to a global store for all instances to share state
    ch_word global_store_size;                  // Size of the global store

} camio_transport_state_t;


//Declare a linked list of key_value items
declare_ch_vector(CAMIO_STREAM_STATE_VEC,camio_transport_state_t)
declare_ch_vector_cmp(CAMIO_STREAM_STATE_VEC,camio_transport_state_t)


#endif /* SRC_TYPES_STREAM_STATE_VEC_H_ */
