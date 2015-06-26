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

typedef struct delim_global_s{
    ch_bool is_init;
    //int delim_fd;
} delim_global_t;


typedef struct {
    len_string_t hierarchical;
} delim_params_t;

void delim_init();

#endif /* SRC_DRIVERS_DELIM_DELIM_TRANSPORT_H_ */
