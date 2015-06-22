/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 16, 2014
 *  File name: netmap_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef NETMAP_STREAM_H_
#define NETMAP_STREAM_H_

#include "../../transports/stream.h"
#include "netmap.h"

NEW_STREAM_DECLARE(netmap);

camio_error_t netmap_stream_construct(camio_stream_t* this, ch_word netmap_fd, struct netmap_if* net_if, ch_word rings_start,
        ch_word rings_end);

#endif /* NETMAP_STREAM_H_ */
