/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   15 Oct 2014
 *  File name: netmap_if.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_NETMAP_NETMAP_IF_H_
#define SRC_DRIVERS_NETMAP_NETMAP_IF_H_

#include "../../types/types.h"
#include "../../../deps/chaste/data_structs/array/array_std.h"

typedef struct netmap_if_s {
    ch_cstr if_name;
    ch_bool sw_tx_ring_avail;
    ch_bool sw_rx_ring_avail;
    CH_ARRAY(ch_bool)* hw_tx_ring_avail;
    CH_ARRAY(ch_bool)* hw_rx_ring_avail;
} netmap_if_t;

#endif /* SRC_DRIVERS_NETMAP_NETMAP_IF_H_ */
