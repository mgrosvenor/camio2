/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_DELIM_DELIM_CHANNEL_H_
#define SRC_DRIVERS_DELIM_DELIM_CHANNEL_H_

#include <src/devices/channel.h>
#include <src/devices/device.h>

NEW_CHANNEL_DECLARE(delim);

camio_error_t delim_channel_construct(
    camio_channel_t* this,
    camio_dev_t* dev,
    camio_channel_t* base_channel,
    ch_word (*delim_fn)(char* buffer, ch_word len),
    ch_word rd_buffs_count
);

#endif /* SRC_DRIVERS_DELIM_DELIM_CHANNEL_H_ */
