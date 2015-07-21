/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_BRING_BRING_TRANSPORT_H_
#define SRC_DRIVERS_BRING_BRING_TRANSPORT_H_


#include "bring_controller.h"
#include "bring_channel.h"

typedef struct bring_global_s{
    ch_bool is_init;
    //int bring_fd;
} bring_global_t;



void bring_init();

#endif /* SRC_DRIVERS_BRING_BRING_TRANSPORT_H_ */
