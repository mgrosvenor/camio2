/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_transport.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_DELIM_DELIM_TRANSPORT_H_
#define SRC_DRIVERS_DELIM_DELIM_TRANSPORT_H_

#include <src/types/len_string.h>

#include "delim_connector.h"

//No global state
//typedef struct delim_global_s{
//} delim_global_t;


typedef struct {
    char* base_uri; //URI for the base stream that this will be delimiting
    int (*delim_fn)(char* buffer, int len); //Delimiting function
} delim_params_t;

void delim_init();

#endif /* SRC_DRIVERS_DELIM_DELIM_TRANSPORT_H_ */
