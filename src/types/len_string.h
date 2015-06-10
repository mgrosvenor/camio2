/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 10, 2015
 *  File name:  len_string.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef LEN_STRING_H_
#define LEN_STRING_H_

#include <deps/chaste/types/types.h>

#define LSTRING_MAX (256)
//A statically allocated string structure to make  memory management easier
typedef struct {
    char str[LSTRING_MAX];
    ch_word str_max;
    ch_word str_len;
} len_string_t;

#endif /* LEN_STRING_H_ */
