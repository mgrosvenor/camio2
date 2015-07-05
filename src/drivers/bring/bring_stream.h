/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_BRING_BRING_STREAM_H_
#define SRC_DRIVERS_BRING_BRING_STREAM_H_

#include <src/transports/stream.h>
#include <src/transports/connector.h>

NEW_STREAM_DECLARE(bring);

typedef struct slot_header_s{
    uint64_t ack       : 1;  //Lets the reader acknowledge the write
    uint64_t _         : 15; //Reserved for future use
    uint64_t data_size : 16; //Maximum slot size of 64k
    uint64_t seq_no_32 : 32; //Order is important! Sequence number for tracking write orderings and loop around
} bring_slot_header_t;

_Static_assert( sizeof(bring_slot_header_t) == sizeof(uint64_t) , "Slot header must be exactly 1 word for atomicity");
_Static_assert( sizeof(void*) == sizeof(uint64_t) , "Machine word must be 64bits");


camio_error_t bring_stream_construct(camio_stream_t* this, camio_connector_t* connector /**, ... , **/);

#endif /* SRC_DRIVERS_BRING_BRING_STREAM_H_ */
