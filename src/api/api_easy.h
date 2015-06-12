/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 12, 2015
 *  File name:  api_easy.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef API_EASY_H_
#define API_EASY_H_

#include <src/camio.h>

/**
 * A nice wrapper around transport creation and connection. Creates a transport and uses the connector to produce a stream.
 * Assumes that the connector will connect immidately.
 */
camio_error_t camio_stream_new(char* uri, camio_stream_t** stream);


#endif /* API_EASY_H_ */
