/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_FIO_FIO_TRANSPORT_H_
#define SRC_DRIVERS_FIO_FIO_TRANSPORT_H_

#include <src/types/len_string.h>

typedef struct fio_global_s{
    ch_bool is_init;
    //int fio_fd;
} fio_global_t;


typedef struct {
    len_string_t hierarchical;
    int64_t rd_buff_sz;
    int64_t wr_buff_sz;
    int64_t rd_fd;
    int64_t wr_fd;
    int64_t rd_only;
    int64_t wr_only;
} fio_params_t;

void fio_init();

#endif /* SRC_DRIVERS_FIO_FIO_TRANSPORT_H_ */
