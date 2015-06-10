/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 6, 2015
 *  File name:  transport_params_vec.h
 *  Description:
 *  Vector description for transport options
 */
#ifndef TRANSPORT_PARAMS_VEC_H_
#define TRANSPORT_PARAMS_VEC_H_

#include "types.h"
#include "../../deps/chaste/data_structs/vector/vector_typed_declare_template.h"

typedef enum {
    CAMIO_TRANSPORT_PARAMS_MODE_REQUIRED,
    CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL,
    //CAMIO_TRANSPORT_PARAMS_MODE_FALG -- is this necessary? Could be added later
} camio_transport_param_mode_e;

typedef enum {
    CAMIO_TRANSPORT_PARAMS_TYPE_UINT64,
    CAMIO_TRANSPORT_PARAMS_TYPE_INT64,
    CAMIO_TRANSPORT_PARAMS_TYPE_DOUBLE,
    CAMIO_TRANSPORT_PARAMS_TYPE_BOOL,
    CAMIO_TRANSPORT_PARAMS_TYPE_STRING,
} camio_transport_param_type_e;


typedef struct {
    camio_transport_param_mode_e opt_mode;
    ch_cstr param_name;
    ch_word param_struct_offset;
    camio_transport_param_type_e type;
    union {
        uint64_t uint64_t_val;
        int64_t int64_t_val;
        double double_val;
        bool bool_val;
        char* ch_cstr_val;
    } default_val;
} camio_transport_param_t;


declare_ch_vector(CAMIO_TRANSPORT_PARAMS_VEC,camio_transport_param_t)
declare_ch_vector_cmp(CAMIO_TRANSPORT_PARAMS_VEC,camio_transport_param_t)

#define declare_add_param(TYPE, CAMIO_TYPE) \
    void add_param_##TYPE(CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* opts, ch_cstr param_name, ch_word param_struct_offset,\
        ch_bool has_default, TYPE default_val )

//TODO XXX!!! There is a confusion here between "parameter" and "option" should rename everything called "option" to
//"parameter"
//These applications of the macro expand out the actual option adder functions
declare_add_param(int64_t, CAMIO_TRANSPORT_PARAMS_TYPE_INT64);
declare_add_param(uint64_t, CAMIO_TRANSPORT_PARAMS_TYPE_UINT64);
declare_add_param(double, CAMIO_TRANSPORT_PARAMS_TYPE_DOUBLE);
declare_add_param(ch_cstr, CAMIO_TRANSPORT_PARAMS_TYPE_STRING);

//TODO XXX!!! There is a confusion here between "parameter" and "option" should rename everything called "option" to
//"parameter"
//This little macro provides a template for expanding option adder code in a type-safe way
#define define_add_param(TYPE, CAMIO_TYPE) \
    declare_add_param(TYPE,CAMIO_TYPE)\
{\
    camio_transport_param_t opt;\
    opt.opt_mode                    = has_default ? CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL : CAMIO_TRANSPORT_PARAMS_MODE_REQUIRED;\
    opt.param_name                    = param_name;\
    opt.param_struct_offset         = param_struct_offset;\
    opt.type                        = CAMIO_TYPE;\
    opt.default_val.TYPE##_val      = default_val; \
    opts->push_back(opts, opt); \
}

//TODO XXX!!! There is a confusion here between "parameter" and "option" should rename everything called "option" to
//"parameter"
//Using the new C11 generic allows me to provide a single, overloaded, typesafe interface to adding options
//This one adds an "optional" option, which means that it must include a default value, and users may or may not populate it.
#define add_param_optional(params_vec, param_name, param_struct, param_struct_memeber, default_val)\
        _Generic( (((type *)0)->member),\
                uint64_t : add_param_uint64_t \
                int64_t  : add_param_int64_t \
                double   : add_param_double \
                ch_cstr  : add_param_ch_cstr \
        )(params_vec, param_name, offsetof(param_struct, param_struct_member), true, default_val)


//TODO XXX!!! There is a confusion here between "parameter" and "option" should rename everything called "option" to
//"parameter"
//This one adds an "required" option, which means that it users must populate it, so no default is required
#define add_param_required(params_vec, param_name, param_struct_type, param_struct_member_name)\
        _Generic( (((param_struct_type *)0)->param_struct_member_name),\
                uint64_t : add_param_uint64_t, \
                int64_t  : add_param_int64_t, \
                double   : add_param_double, \
                ch_cstr  : add_param_ch_cstr \
        )(params_vec, param_name, offsetof(param_struct_type, param_struct_member_name), false, 0)


#endif /* TRANSPORT_PARAMS_VEC_H_ */
