/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_transport.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_UDP_UDP_TRANSPORT_H_
#define SRC_DRIVERS_UDP_UDP_TRANSPORT_H_

#include <src/drivers/udp/udp_connector.h>
#include <src/types/len_string.h>

typedef struct udp_global_s{
    ch_bool is_init;
    int udp_fd;
} udp_global_t;


typedef struct {
    len_string_t hierarchical;
    len_string_t rd_address;
    len_string_t wr_address;
    len_string_t rd_protocol;
    len_string_t wr_protocol;
} udp_params_t;

void udp_init();

#endif /* SRC_DRIVERS_UDP_UDP_TRANSPORT_H_ */
