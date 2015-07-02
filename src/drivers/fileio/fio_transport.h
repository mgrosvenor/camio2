/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_transport.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_FIO_FIO_TRANSPORT_H_
#define SRC_DRIVERS_FIO_FIO_TRANSPORT_H_

#include <src/types/len_string.h>

#include "fio_connector.h"

typedef struct fio_global_s{
    ch_bool is_init;
    //int fio_fd;
} fio_global_t;


typedef struct {
    len_string_t hierarchical;
} fio_params_t;

void fio_init();

#endif /* SRC_DRIVERS_FIO_FIO_TRANSPORT_H_ */
