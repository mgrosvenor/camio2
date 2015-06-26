/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_DELIM_DELIM_STREAM_H_
#define SRC_DRIVERS_DELIM_DELIM_STREAM_H_

#include <src/transports/stream.h>
#include <src/transports/connector.h>

NEW_STREAM_DECLARE(delim);

camio_error_t delim_stream_construct(camio_stream_t* this, camio_connector_t* connector, camio_stream_t* base_stream);

#endif /* SRC_DRIVERS_DELIM_DELIM_STREAM_H_ */
