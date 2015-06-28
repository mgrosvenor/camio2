/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   15 Oct 2014
 *  File name: camio_debug.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_CAMIO_DEBUG_H_
#define SRC_CAMIO_DEBUG_H_

#include <unistd.h>
#include "types/types.h"


#ifndef NDEBUG
    ch_word camio_debug_out_(ch_bool info, ch_word line_num, const char* filename, const char* function,  const char* format, ... );
    #define DBG( /*format, args*/...)  camio_debug_helper(__VA_ARGS__, "")
    #define camio_debug_helper(format, ...) camio_debug_out_(true,__LINE__, __FILE__, __FUNCTION__, format, __VA_ARGS__ )
    #define DBG2( /*format, args*/...)  camio_debug_helper2(__VA_ARGS__, "")
    #define camio_debug_helper2(format, ...) camio_debug_out_(false,__LINE__, __FILE__, __FUNCTION__, format, __VA_ARGS__ )
#else
    #define DBG( /*format, args*/...)
    #define DBG2( /*format, args*/...)
#endif


#endif /* SRC_CAMIO_DEBUG_H_ */
