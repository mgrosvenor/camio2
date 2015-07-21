/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: stdio_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_STDIO_STDIO_TRANSPORT_H_
#define SRC_DRIVERS_STDIO_STDIO_TRANSPORT_H_

#include <src/types/len_string.h>

#include "stdio_controller.h"
#include <src/drivers/fileio/fio_device.h>

typedef struct stdio_global_s{
    ch_bool is_init;
    //int stdio_fd;
} stdio_global_t;


//Just redefine the file io params as stdio
typedef struct {
    len_string_t __hierarchical__; //This is not used
    int64_t rd_buff_sz;
    int64_t wr_buff_sz;
    int64_t rd_only;
    int64_t wr_only;
    int64_t std_err; //Output to stderr
} stdio_params_t;

void stdio_init();

#endif /* SRC_DRIVERS_STDIO_STDIO_TRANSPORT_H_ */
