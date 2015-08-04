/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_TTT_TTT_CHANNEL_H_
#define SRC_DRIVERS_TTT_TTT_CHANNEL_H_

#include <src/devices/channel.h>
#include <src/devices/device.h>

NEW_CHANNEL_DECLARE(ttt);

camio_error_t ttt_channel_construct(camio_channel_t* this, camio_dev_t* dev /**, ... , **/);

#endif /* SRC_DRIVERS_TTT_TTT_CHANNEL_H_ */
