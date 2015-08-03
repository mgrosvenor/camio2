/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_FIO_FIO_CHANNEL_H_
#define SRC_DRIVERS_FIO_FIO_CHANNEL_H_

#include <src/devices/channel.h>
#include <src/devices/device.h>

#include "fio_device.h"

NEW_CHANNEL_DECLARE(fio);

camio_error_t fio_channel_construct(camio_channel_t* this, camio_device_t* device, fio_params_t* params, int rd_fd, int wr_fd);

#endif /* SRC_DRIVERS_FIO_FIO_CHANNEL_H_ */
