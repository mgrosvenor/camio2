// CamIO 2: buffer.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef BUFFER_H_
#define BUFFER_H_

#include <libm6/libm6.h>

#define CAMIO2_BUFFER_NO_ERROR (0)
#define CAMIO2_BUFFER_NO_FREESLOTS (-1)
#define CAMIO2_BUFFER_NULL_POINTER (-2)
#define CAMIO2_BUFFER_OUT_OF_RANGE (-3)
#define CAMIO2_BUFFER_INVALID_SLOT_ID (-4)

struct _camio2_buffer;
typedef struct _camio2_buffer camio2_buffer_t;

struct _camio2_buffer {
    i64 (*reserve)(camio2_buffer_t* this, i64* slot_id, i64* slot_size, u8** slot);
    i64 (*release)(camio2_buffer_t* this, const i64 slot_id);
    void (*cdelete)(camio2_buffer_t* this);
    void* priv;
};


#endif /* BUFFER_H_ */
