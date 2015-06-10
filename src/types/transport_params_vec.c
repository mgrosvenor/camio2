/*
 * CamIO - The Cambridge Input/Output API
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details.
 *
 *  Created:    Jun 6, 2015
 *  File name:  transport_opts_vec.c
 *  Description:
 *  Vector description for transport options
 */

#include "transport_params_vec.h"
#include "../../deps/chaste/data_structs/vector/vector_typed_define_template.h"


define_ch_vector(CAMIO_TRANSPORT_PARAMS_VEC,camio_transport_param_t)

//Don't bother to define this yet.
//define_ch_vector_compare(CAMIO_TRANSPORT_PARAMS_VEC,camio_transport_opt_t)
//{
//    return
//}


//These applications of the macro expand out the actual option adder functions
define_add_param(int64_t, CAMIO_TRANSPORT_PARAMS_TYPE_INT64)
define_add_param(uint64_t, CAMIO_TRANSPORT_PARAMS_TYPE_UINT64)
define_add_param(double, CAMIO_TRANSPORT_PARAMS_TYPE_DOUBLE)

//Strings are treated a little bit differently
declare_add_param_opt(ch_cstr,CAMIO_TRANSPORT_PARAMS_TYPE_LSTRING)
{
    camio_transport_param_t opt = {0};
    opt.opt_mode                    = CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL;
    opt.param_name                  = param_name;
    opt.param_struct_offset         = param_struct_offset;
    opt.type                        = CAMIO_TRANSPORT_PARAMS_TYPE_LSTRING;
    len_string_t tmp = {
            .str = {0},
            .str_max = LSTRING_MAX,
            .str_len = strlen(default_val),
    };
    strncpy(tmp.str,default_val, tmp.str_max);
    opt.default_val.len_string_t_val = tmp;
    opts->push_back(opts, opt);
}

define_add_param_req(ch_cstr,CAMIO_TRANSPORT_PARAMS_TYPE_LSTRING)
