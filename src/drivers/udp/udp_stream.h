/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_UDP_UDP_STREAM_H_
#define SRC_DRIVERS_UDP_UDP_STREAM_H_

#include "../../transports/stream.h"

NEW_STREAM_DECLARE(udp);

camio_error_t udp_stream_construct(camio_stream_t* this, ch_word netmap_fd);

#endif /* SRC_DRIVERS_UDP_UDP_STREAM_H_ */
