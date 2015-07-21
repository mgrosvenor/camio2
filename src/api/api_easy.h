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
#include <src/devices/channel.h>

/**
 * A nice wrapper around device creation and connection. Creates a device and uses the controller to produce a channel.
 * Assumes that the controller will connect immidately.
 */
camio_error_t camio_channel_new(char* uri, camio_channel_t** channel);

camio_error_t camio_controller_new(char* uri, camio_controller_t** controller_o);


#endif /* API_EASY_H_ */
