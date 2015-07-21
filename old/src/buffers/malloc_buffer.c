/*
 * malloc_buffer.c
 *
 *  Created on: Mar 5, 2014
 *      Author: mgrosvenor
 */


/*
 * malloc_buffer.h
 *
 *  Created on: Mar 5, 2014
 *      Author: mgrosvenor
 */

#include "slot.h"
#include "../errors/errors.h"
#include "unistd.h"
#include "malloc_buffer.h"


int new_malloc_buffer(uint64_t slot_count, uint64_t slot_size, malloc_buffer** buff_o)
{
	malloc_buffer* result = (malloc_buffer*)calloc(1,sizeof(malloc_buffer));
	if(!result){
		return CIO_ENOMEM;
	}

	//Round up request to the nearest multiple of the cacheline size
	const u64 slot_size_aligned =  (slot_size + (L1_CACHELINE_SIZE -1)) / L1_CACHELINE_SIZE;

    //Add an extra page to the allocation so that we can page align later
	u64 buff_size 	 = slot_count * slot_size_aligned + getpagesize();


	result->__buffer_mem     = malloc( buff_size );
	if(!result){
		free(result);
		return CIO_ENOMEM;
	}
	//OK. We have memory, now page align it.
	//Just mask out the lower bits and we're ready to go
	const uint64_t mask = ~(getpagesize() - 1);
	result->buffer_mem_aligned = uint64_t(result->__buffer_mem) &  mask;

	//Make the slot pointers
	result->slots = calloc(slot_count,sizeof(cioslot));
	if(!result->slots){
		free(result->__buffer_mem);
		free(result);
		return CIO_ENOMEM;
	}
	result->slots = slot_count;

	//Populate them with sensible defaults.
	//Each channel should override with specific settings
	for(int i = 0; i < result->slot_count; i++){
		result->slots[i].__buffer_id    = (uint64_t)result;
		result->slots[i].__in_use       = false;
		result->slots[i].__do_release   = false;
		result->slots[i].__slot_id      = i;
		result->slots[i].slot_len       = slot_size_aligned;
		result->slots[i].slot_start     = (char*)result->buffer_mem_aligned + slot_size_aligned * i;
		result->slots[i].data_start     = NULL;
		result->slots[i].read_len       = 0;
		result->slots[i].timestamp_type = CIO_SLOT_TST_NONE;
		result->slots[i].ts				= NULL;
		result->slots[i].valid			= false;
	}

	*buff_o = result;

	return CIO_ENOERROR;

}


void free_malloc_buffer(malloc_buffer* buff)
{
	if(buff->__buffer_mem){
		free(buff->__buffer_mem);
	}

	if(buff->slots){
		free(buff->slots);

	}

	free(buff);

}

