/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jul 7, 2015
 *  File name:  camio_perf_packet.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef CAMIO_PERF_PACKET_H_
#define CAMIO_PERF_PACKET_H_

typedef struct camio_perf_packet_head_s {
    ch_word size;
    ch_word seq_number;
    ch_word time_stamp;
} camio_perf_packet_head_t;


#endif /* CAMIO_PERF_PACKET_H_ */
