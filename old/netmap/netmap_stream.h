/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 16, 2014
 *  File name: netmap_channel.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef NETMAP_STREAM_H_
#define NETMAP_STREAM_H_

#include "../../devices/channel.h"
#include "netmap.h"

NEW_STREAM_DECLARE(netmap);

camio_error_t netmap_channel_construct(camio_channel_t* this, ch_word netmap_fd, struct netmap_if* net_if, ch_word rings_start,
        ch_word rings_end);

#endif /* NETMAP_STREAM_H_ */
