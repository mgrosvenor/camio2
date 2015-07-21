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
#ifndef SRC_DRIVERS_TTT_TTT_STREAM_H_
#define SRC_DRIVERS_TTT_TTT_STREAM_H_

#include <src/devices/channel.h>
#include <src/devices/controller.h>

NEW_STREAM_DECLARE(ttt);

camio_error_t ttt_channel_construct(camio_channel_t* this, camio_controller_t* controller /**, ... , **/);

#endif /* SRC_DRIVERS_TTT_TTT_STREAM_H_ */
