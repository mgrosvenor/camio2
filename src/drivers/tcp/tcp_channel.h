/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   22 Jun 2015
 *  File name: tcp_channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_TCP_TCP_STREAM_H_
#define SRC_DRIVERS_TCP_TCP_STREAM_H_

#include <src/devices/channel.h>
#include <src/devices/controller.h>
#include "tcp_device.h"

NEW_STREAM_DECLARE(tcp);

camio_error_t tcp_channel_construct(camio_channel_t* this, camio_controller_t* controller, tcp_params_t* params, int fd);

#endif /* SRC_DRIVERS_TCP_TCP_STREAM_H_ */
