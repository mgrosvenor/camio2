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
#ifndef SRC_DRIVERS_FIO_FIO_DEVICE_H_
#define SRC_DRIVERS_FIO_FIO_DEVICE_H_

#include <src/devices/device.h>
#include "fio_device.h"


NEW_DEVICE_DECLARE(fio);

/**************************************************************************************************************************
 * PER CHANNEL STATE
 **************************************************************************************************************************/
typedef struct fio_priv_s {

    fio_params_t* params;  //Parameters used when a connection happens

    bool is_devected;          //Has devect be called?
    int base_rd_fd;             //File descriptor for reading
    int base_wr_fd;             //File descriptor for writing

} fio_device_priv_t;


#endif /* SRC_DRIVERS_FIO_FIO_DEVICE_H_ */
