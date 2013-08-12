// CamIO 2: buffer_malloc.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

//A simple malloc based buffer.

#include "buffer.h"
#include <libm6/libm6.h>

typedef struct {
    void* buffer;
    i64 slot_id;
    i64 reserved;
    i64 padding; //Pad this out to a nice power of two so that we cache friendly
} camio2_buffer_malloc_slot_t;


typedef struct {
    camio2_buffer_t this;
    camio2_buffer_malloc_slot_t* slots;
    u8* buffers;
    i64 slot_count;
    i64 slot_size;

} camio2_buffer_malloc_priv_t ;

camio2_buffer_t* camio2_buffer_malloc_new(i64 slot_size, i64 slot_count);
