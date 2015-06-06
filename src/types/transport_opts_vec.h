/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 6, 2015
 *  File name:  transport_opts_vec.h
 *  Description:
 *  Vector description for transport options
 */
#ifndef TRANSPORT_OPTS_VEC_H_
#define TRANSPORT_OPTS_VEC_H_

#include "types.h"
#include "../../deps/chaste/data_structs/vector/vector_typed_declare_template.h"

typedef enum {
    CAMIO_TRANSPORT_OPT_MODE_REQUIRED,
    CAMIO_TRANSPORT_OPT_MODE_OPTIONAL,
    //CAMIO_TRANSPORT_OPT_MODE_FALG -- is this necessary? Could be added later
} camio_transport_opt_mode_e;

typedef enum {
    CAMIO_TRANSPORT_OPT_TYPE_UINT64,
    CAMIO_TRANSPORT_OPT_TYPE_INT64,
    CAMIO_TRANSPORT_OPT_TYPE_DOUBLE,
    CAMIO_TRANSPORT_OPT_TYPE_BOOL,
    CAMIO_TRANSPORT_OPT_TYPE_STRING,
} camio_transport_opt_type_e;


typedef struct {
    camio_transport_opt_mode_e opt_mode;
    ch_cstr opt_name;
    ch_word param_struct_offset;
    camio_transport_opt_type_e type;
} camio_transport_opt_t;


declare_ch_vector(CAMIO_TRANSPORT_OPT_VEC,camio_transport_opt_t)
declare_ch_vector_cmp(CAMIO_TRANSPORT_OPT_VEC,camio_transport_opt_t)


#endif /* TRANSPORT_OPTS_VEC_H_ */
