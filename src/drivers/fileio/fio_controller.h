/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_controller.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_FIO_FIO_CONNECTOR_H_
#define SRC_DRIVERS_FIO_FIO_CONNECTOR_H_

#include <src/devices/controller.h>
#include "fio_device.h"


NEW_CONNECTOR_DECLARE(fio);

/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct fio_priv_s {

    fio_params_t* params;  //Parameters used when a connection happens

    bool is_connected;          //Has connect be called?
    int base_rd_fd;             //File descriptor for reading
    int base_wr_fd;             //File descriptor for writing

} fio_controller_priv_t;


#endif /* SRC_DRIVERS_FIO_FIO_CONNECTOR_H_ */
