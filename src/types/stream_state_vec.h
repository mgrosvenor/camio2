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
#include "../../deps/chaste/data_structs/vector/vector_typed_declare_template.h"


typedef struct {
    uint64_t;
    uint64_t;
} camio_uuid_t;


typedef struct camio_stream_state_s {
    ch_cstr scheme;                                         // The short name for this stream eg: "tcp" or "udp"
    ch_word scheme_len;

    camio_stream_t* (*construct_str)( camio_uri_t* uri);    // Construct from a string
    camio_stream_t* (*construct_bin)( va_list args );       // Construct from a binary

    void* global_store;                                     // Pointer to a global store for all instances to share state
} camio_stream_state_t;


//Declare a linked list of key_value items
declare_ch_vector(CAMIO_STREAM_STATE_VEC,camio_stream_state_t)
declare_ch_vector_cmp(CAMIO_STREAM_STATE_VEC,camio_stream_state_t);


#endif /* SRC_TYPES_STREAM_STATE_VEC_H_ */
