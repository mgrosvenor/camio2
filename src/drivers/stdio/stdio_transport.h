/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: stdio_transport.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_STDIO_STDIO_TRANSPORT_H_
#define SRC_DRIVERS_STDIO_STDIO_TRANSPORT_H_

#include <src/types/len_string.h>

#include "stdio_connector.h"
#include <src/drivers/fileio/fio_transport.h>

typedef struct stdio_global_s{
    ch_bool is_init;
    //int stdio_fd;
} stdio_global_t;


//Just redefine the file io params as stdio
typedef struct {
    len_string_t __hierarchical__; //This is not used
    ch_word rd_buff_sz;
    ch_word wr_buff_sz;
    ch_word rd_only;
    ch_word wr_only;
    ch_word std_err; //Output to stderr
} stdio_params_t;

void stdio_init();

#endif /* SRC_DRIVERS_STDIO_STDIO_TRANSPORT_H_ */
