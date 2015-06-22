/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   22 Jun 2015
 *  File name: tcp_transport.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_TCP_TCP_TRANSPORT_H_
#define SRC_DRIVERS_TCP_TCP_TRANSPORT_H_

#include <src/drivers/tcp/tcp_connector.h>
#include <src/types/len_string.h>

typedef struct tcp_global_s{
    ch_bool is_init;
} tcp_global_t;


typedef struct {
    len_string_t hierarchical;
    int64_t listen;
} tcp_params_t;

void tcp_init();

#endif /* SRC_DRIVERS_TCP_TCP_TRANSPORT_H_ */
