/*
 * malloc_buffer.h
 *
 *  Created on: Mar 5, 2014
 *      Author: mgrosvenor
 */

#ifndef MALLOC_BUFFER_H_
#define MALLOC_BUFFER_H_

#include "slot.h"
#include "../errors/errors.h"
#include "unistd.h"

//TODO XXX: This should be passed in as a compile time define.
//On MacOS X: sysctl hw.cachlinesize
//On linux:
#ifndef L1_CACHELINE_SIZE
#define L1_CACHELINE_SIZE 64
#endif


typedef struct {
	void* __buffer_mem;
	void* buffer_mem_aligned;
	cioslot* slots;
	uint64_t slot_size;
	uint64_t slot_count;
} malloc_buffer;


int new_malloc_buffer(uint64_t slot_count, uint64_t slot_size, malloc_buffer** buff_o);
void free_malloc_buffer(malloc_buffer* buff);

#endif /* MALLOC_BUFFER_H_ */
