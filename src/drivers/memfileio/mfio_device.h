/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   01 Jul 2015
 *  File name: mfio_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_MFIO_MFIO_TRANSPORT_H_
#define SRC_DRIVERS_MFIO_MFIO_TRANSPORT_H_

#include <src/types/len_string.h>

#include "mfio_device.h"

typedef struct mfio_global_s{
    ch_bool is_init;
    //int mfio_fd;
} mfio_global_t;


typedef struct {
    len_string_t hierarchical;
} mfio_params_t;

void mfio_init();

#endif /* SRC_DRIVERS_MFIO_MFIO_TRANSPORT_H_ */
