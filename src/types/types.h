/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 2014
 *  File name: types.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef SRC_TYPES_TYPES_H_
#define SRC_TYPES_TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <deps/chaste/types/types.h>
#include <src/errors/camio_errors.h>

//Forward declaration, the real definition is found in channels/features.h
struct camio_channel_s;
typedef struct camio_channel_s camio_channel_t;


//Forward declaration, the real definition is found in channels/channel.h
struct camio_channel_s;
typedef struct camio_channel_s camio_channel_t;

//Forward declaration, the real definition is found in channels/device.h
struct camio_device_s;
typedef struct camio_device_s camio_device_t;

//Forward declaration, the real definition is found in channels/device.h
struct camio_device_interface_s;
typedef struct camio_device_interface_s camio_device_interface_t;

//Forward declaration, the real definition is found in buffers/buffer.h
struct camio_buffer_s;
typedef struct camio_buffer_s camio_buffer_t;

//Forward declaration, the real definition is found in muxors/muxable.h
struct camio_muxable_s;
typedef struct camio_muxable_s camio_muxable_t;

//Forward declaration, the real definition is found in muxors/muxor.h
struct camio_mux_s;
typedef struct camio_mux_s camio_mux_t;


#endif /* SRC_TYPES_TYPES_H_ */
