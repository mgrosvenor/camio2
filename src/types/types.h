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

#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "../../deps/chaste/types/types.h"
#include "../errors/errors.h"

//Forward declaration, the real definition is found in streams/features.h
struct camio_stream_s;
typedef struct camio_stream_s camio_stream_t;


//Forward declaration, the real definition is found in streams/stream.h
struct camio_stream_s;
typedef struct camio_stream_s camio_stream_t;

//Forward declaration, the real definition is found in streams/connector.h
struct camio_connector_s;
typedef struct camio_connector_s camio_connector_t;

//Forward declaration, the real definition is found in streams/connector.h
struct camio_connector_interface_s;
typedef struct camio_connector_interface_s camio_connector_interface_t;

//Forward declaration, the real definition is found in buffers/buffer.h
struct camio_buffer_s;
typedef struct camio_buffer_s camio_buffer_t;

//Forward declaration, the real definition is found in selectors/selectable.h
struct camio_selectable_s;
typedef struct camio_selectable_s camio_selectable_t;
#endif /* TYPES_H_ */
