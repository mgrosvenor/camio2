/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   22 Jun 2015
 *  File name: tcp_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_TCP_TCP_STREAM_H_
#define SRC_DRIVERS_TCP_TCP_STREAM_H_

#include <src/transports/stream.h>
#include <src/transports/connector.h>
#include "tcp_transport.h"

NEW_STREAM_DECLARE(tcp);

camio_error_t tcp_stream_construct(camio_stream_t* this, camio_connector_t* connector, tcp_params_t* params, int fd);

#endif /* SRC_DRIVERS_TCP_TCP_STREAM_H_ */
