/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) ZZZZ, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   PPP ZZZZ
 *  File name: ttt_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_TTT_TTT_TRANSPORT_H_
#define SRC_DRIVERS_TTT_TTT_TRANSPORT_H_

#include <src/types/len_string.h>

#include "ttt_device.h"

typedef struct ttt_global_s{
    ch_bool is_init;
    //int ttt_fd;
} ttt_global_t;


typedef struct {
    len_string_t hierarchical;
} ttt_params_t;

void ttt_init();

#endif /* SRC_DRIVERS_TTT_TTT_TRANSPORT_H_ */
