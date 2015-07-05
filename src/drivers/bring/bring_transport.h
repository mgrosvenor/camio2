/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_transport.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_BRING_BRING_TRANSPORT_H_
#define SRC_DRIVERS_BRING_BRING_TRANSPORT_H_

#include <src/types/len_string.h>

#include "bring_connector.h"

typedef struct bring_global_s{
    ch_bool is_init;
    //int bring_fd;
} bring_global_t;


typedef struct {
    len_string_t hierarchical;
    int64_t slots;
    int64_t slot_sz;
    int64_t blocking;
    int64_t server;
    int64_t rd_only;
    int64_t wr_only;
    int64_t expand;
} bring_params_t;

void bring_init();

#endif /* SRC_DRIVERS_BRING_BRING_TRANSPORT_H_ */
