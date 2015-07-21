/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 6, 2015
 *  File name:  device_params_vec.h
 *  Description:
 *  Vector description for device options
 */
#ifndef TRANSPORT_PARAMS_VEC_H_
#define TRANSPORT_PARAMS_VEC_H_

#include "types.h"
#include <deps/chaste/data_structs/vector/vector_typed_declare_template.h>
#include <src/types/len_string.h>

typedef enum {
    CAMIO_TRANSPORT_PARAMS_MODE_REQUIRED,
    CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL,
    //CAMIO_TRANSPORT_PARAMS_MODE_FALG -- is this necessary? Could be added later
} camio_device_param_mode_e;

typedef enum {
    CAMIO_TRANSPORT_PARAMS_TYPE_UINT64,
    CAMIO_TRANSPORT_PARAMS_TYPE_INT64,
    CAMIO_TRANSPORT_PARAMS_TYPE_DOUBLE,
    CAMIO_TRANSPORT_PARAMS_TYPE_LSTRING,
} camio_device_param_type_e;



typedef struct {
    camio_device_param_mode_e opt_mode;  //Is this an optional or required parameter
    ch_cstr param_name;                     //What is the text name given in the URI for it?
    ch_word param_struct_offset;            //How deep in the parameter structure is this found?
    camio_device_param_type_e type;      //What type of variable is it, current int, uint, double and string are supported
    ch_bool found;                          //Has the parser found an instance of this parameter? There should be only 1
    union {                                 //For optional parameters, we need a default value somewhere
        uint64_t uint64_t_val;
        int64_t int64_t_val;
        double double_val;
        bool bool_val;
        len_string_t len_string_t_val;
    } default_val;

} camio_device_param_t;


declare_ch_vector(CAMIO_TRANSPORT_PARAMS_VEC,camio_device_param_t)
declare_ch_vector_cmp(CAMIO_TRANSPORT_PARAMS_VEC,camio_device_param_t)

//Declare optional parameters
#define declare_add_param_opt(TYPE, CAMIO_TYPE) \
    void add_param_##TYPE##_opt(CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* opts, ch_cstr param_name, ch_word param_struct_offset,\
        TYPE default_val )

//Declare required parameters
#define declare_add_param_req(TYPE, CAMIO_TYPE) \
    void add_param_##TYPE##_req(CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* opts, ch_cstr param_name, ch_word param_struct_offset)

//Declare both the required and optional parameters in one place
#define declare_add_param(TYPE, CAMIO_TYPE) \
    declare_add_param_opt(TYPE, CAMIO_TYPE);\
    declare_add_param_req(TYPE, CAMIO_TYPE);\

//These applications of the macro expand out the actual parameter
declare_add_param(int64_t, CAMIO_TRANSPORT_PARAMS_TYPE_INT64)
declare_add_param(uint64_t, CAMIO_TRANSPORT_PARAMS_TYPE_UINT64)
declare_add_param(double, CAMIO_TRANSPORT_PARAMS_TYPE_DOUBLE)
declare_add_param(ch_cstr, CAMIO_TRANSPORT_PARAMS_TYPE_LSTRING)

//This little macro provides a template for expanding parameter adder code in a type-safe way
#define define_add_param_opt(TYPE, CAMIO_TYPE) \
declare_add_param_opt(TYPE,CAMIO_TYPE)\
{\
    camio_device_param_t opt = {0};\
    opt.opt_mode                    =  CAMIO_TRANSPORT_PARAMS_MODE_OPTIONAL;\
    opt.param_name                  = param_name;\
    opt.param_struct_offset         = param_struct_offset;\
    opt.type                        = CAMIO_TYPE;\
    opt.default_val.TYPE##_val      = default_val; \
    opts->push_back(opts, opt); \
}

//This little macro provides a template for expanding parameter adder code in a type-safe way
#define define_add_param_req(TYPE, CAMIO_TYPE) \
declare_add_param_req(TYPE,CAMIO_TYPE)\
{\
    camio_device_param_t opt = {0};\
    opt.opt_mode                    = CAMIO_TRANSPORT_PARAMS_MODE_REQUIRED;  \
    opt.param_name                  = param_name;\
    opt.param_struct_offset         = param_struct_offset;\
    opt.type                        = CAMIO_TYPE;\
    opts->push_back(opts, opt); \
}

//Expand out both the required and optional parameters
#define define_add_param(TYPE, CAMIO_TYPE) \
        define_add_param_opt(TYPE, CAMIO_TYPE)\
        define_add_param_req(TYPE, CAMIO_TYPE)\


//Using the new C11 generic allows me to provide a single, overloaded, typesafe interface to adding options
//This one adds an "optional" option, which means that it must include a default value, and users may or may not populate it.
#define add_param_optional(params_vec, param_name, param_struct_type, param_struct_member_name, default_val)\
        _Generic( (((param_struct_type *)0)->param_struct_member_name),\
                uint64_t    : add_param_uint64_t_opt, \
                int64_t     : add_param_int64_t_opt, \
                double      : add_param_double_opt, \
                len_string_t: add_param_ch_cstr_opt \
        )(params_vec, param_name, offsetof(param_struct_type, param_struct_member_name), default_val)


//This one adds a "required" option, which means that it users must populate it, so no default is required
#define add_param_required(params_vec, param_name, param_struct_type, param_struct_member_name)\
        _Generic( (((param_struct_type *)0)->param_struct_member_name),\
                uint64_t    : add_param_uint64_t_req, \
                int64_t     : add_param_int64_t_req, \
                double      : add_param_double_req, \
                len_string_t: add_param_ch_cstr_req \
        )(params_vec, param_name, offsetof(param_struct_type, param_struct_member_name))


#endif /* TRANSPORT_PARAMS_VEC_H_ */
