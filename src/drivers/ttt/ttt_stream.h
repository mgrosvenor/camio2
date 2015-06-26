/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_TTT_TTT_STREAM_H_
#define SRC_DRIVERS_TTT_TTT_STREAM_H_

#include <src/transports/stream.h>
#include <src/transports/connector.h>

NEW_STREAM_DECLARE(ttt);

camio_error_t ttt_stream_construct(camio_stream_t* this, camio_connector_t* connector /**, ... , **/);

#endif /* SRC_DRIVERS_TTT_TTT_STREAM_H_ */
