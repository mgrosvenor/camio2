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
#include <src/types/len_string.h>

NEW_STREAM_DECLARE(bring);

typedef struct slot_header_s{
    ch_word data_size;
    ch_word seq_no;
} bring_slot_header_t;

#define BRING_SEQ_MASK (~0xFFFFFFFFULL)

_Static_assert(
    (sizeof(bring_slot_header_t) / sizeof(u64)) * sizeof(u64) == sizeof(bring_slot_header_t),
    "Slot header must be a multiple of 1 word for atomicity"
);
_Static_assert( sizeof(void*) == sizeof(uint64_t) , "Machine word must be 64bits");

//Header structure -- There is one of these for every
#define BRING_MAGIC_SERVER 0xC5f7C37C69627EFULL //Any value here is good as long as it's not zero
#define BRING_MAGIC_CLIENT ~(BRING_MAGIC_SERVER) //Any calue here is good as long as it's not zero and not the same as above

typedef struct bring_header_s {
    ch_word magic;                  //Is this memory ready yet?
    ch_word total_mem;              //Total amount of memory needed in the mmapped region

    ch_word rd_mem_start_offset;    //location of the memory region for reads
    ch_word rd_mem_len;             //length of the read memory region
    ch_word rd_slots;               //number of slots in the read region
    ch_word rd_slots_size;          //Size of each slot including the slot footer

    ch_word wr_mem_start_offset;
    ch_word wr_mem_len;
    ch_word wr_slots;
    ch_word wr_slots_size;
} bring_header_t;


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

camio_error_t bring_stream_construct(
    camio_stream_t* this,
    camio_controller_t* connector,
    volatile bring_header_t* bring_head,
    bring_params_t* params,
    int fd
);

#endif /* SRC_DRIVERS_BRING_BRING_STREAM_H_ */
