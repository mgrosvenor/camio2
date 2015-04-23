/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: camio.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef CAMIO_H_
#define CAMIO_H_

#include <src/api/api.h>
#include <stdint.h>
#include <src/types/stream_state_vec.h>

//Container for all CamIO state
typedef struct camio_s {
    ch_bool is_initialized;                             //Has the CamIO state been initialized?
    CH_VECTOR(CAMIO_STREAM_STATE_VEC)* stream_state;    //Container for all of the streams to be registered. For the moment
                                                        //this is a vector, but it should be a hashmap later.
} camio_t;

extern camio_t __camio_state_container;

#define USE_CAMIO camio_t __camio_state_container = { 0 }

camio_t* init_camio();

camio_error_t register_new_transport(ch_ccstr scheme, ch_word scheme_len, camio_construct_str_f construct_str,
    camio_construct_bin_f construct_bin, ch_word global_store_size);

camio_error_t new_camio_transport_str( ch_cstr uri, camio_transport_features_t* features, camio_connector_t** connector_o);

camio_error_t new_camio_transport_bin(ch_ccstr scheme, camio_transport_features_t* features,
    camio_connector_t** connector_o, ...);

camio_error_t camio_transport_get_global(ch_ccstr scheme, void** global_store);



#endif /* CAMIO_H_ */
