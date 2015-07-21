/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   01 Jul 2015
 *  File name: mfio_channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_MFIO_MFIO_CHANNEL_H_
#define SRC_DRIVERS_MFIO_MFIO_CHANNEL_H_

#include <src/devices/channel.h>
#include <src/devices/controller.h>

NEW_CHANNEL_DECLARE(mfio);

camio_error_t mfio_channel_construct(
    camio_channel_t* this,
    camio_controller_t* controller,
    int base_fd,
    void* base_mem_start,
    ch_word base_mem_len
);

#endif /* SRC_DRIVERS_MFIO_MFIO_CHANNEL_H_ */
