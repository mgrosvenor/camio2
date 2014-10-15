/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 16, 2014
 *  File name: netmap_transport.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef NETMAP_TRANSPORT_H_
#define NETMAP_TRANSPORT_H_

#include <src/drivers/netmap/netmap_connector.h>

typedef struct netmap_global_s{
    ch_bool is_init;
    int netmap_fd;
} netmap_global_t;


void netmap_init();

#endif /* NETMAP_TRANSPORT_H_ */
