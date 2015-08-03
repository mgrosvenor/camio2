/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_DELIM_DELIM_TRANSPORT_H_
#define SRC_DRIVERS_DELIM_DELIM_TRANSPORT_H_

#include <src/types/len_string.h>

#include "delim_device.h"

//No global state
//typedef struct delim_global_s{
//} delim_global_t;


typedef struct {
    char* base_uri; //URI for the base channel that this will be delimiting
    ch_word (*delim_fn)(char* buffer, ch_word len); //Delimiting function
    ch_word rd_buffs;
} delim_params_t;

void delim_init();

#endif /* SRC_DRIVERS_DELIM_DELIM_TRANSPORT_H_ */
