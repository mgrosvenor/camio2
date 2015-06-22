/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   16 Nov 2014
 *  File name: netmap_buffer.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_NETMAP_NETMAP_BUFFER_H_
#define SRC_DRIVERS_NETMAP_NETMAP_BUFFER_H_

#include "../../buffers/buffer.h"
#include "netmap_user.h"


CALLOC_BUFFER_DECLARE(netmap);

typedef struct netmap_buffer_priv_s {
    ch_word ring_idx;
    ch_word slot_idx;
} netmap_buffer_priv_t;

#endif /* SRC_DRIVERS_NETMAP_NETMAP_BUFFER_H_ */
