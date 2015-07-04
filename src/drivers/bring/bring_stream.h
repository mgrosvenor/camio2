/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_BRING_BRING_STREAM_H_
#define SRC_DRIVERS_BRING_BRING_STREAM_H_

#include <src/transports/stream.h>
#include <src/transports/connector.h>

NEW_STREAM_DECLARE(bring);

camio_error_t bring_stream_construct(camio_stream_t* this, camio_connector_t* connector /**, ... , **/);

#endif /* SRC_DRIVERS_BRING_BRING_STREAM_H_ */
