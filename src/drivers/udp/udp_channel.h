/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_UDP_UDP_CHANNEL_H_
#define SRC_DRIVERS_UDP_UDP_CHANNEL_H_

#include <src/devices/channel.h>
#include <src/devices/controller.h>
#include "udp_device.h"

NEW_CHANNEL_DECLARE(udp);

camio_error_t udp_channel_construct(camio_channel_t* this, camio_controller_t* controller, udp_params_t* params, int rd_fd, int wr_fd);

#endif /* SRC_DRIVERS_UDP_UDP_CHANNEL_H_ */
