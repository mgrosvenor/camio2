// CamIO 2: buffer_malloc.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#include <unistd.h>
#include <strings.h>

#include "buffer_malloc.h"
#include "../../deps/libm6/libm6.h"

//Use forward declarations so real declarations can be in any order
static i64 reserve(camio2_buffer_t* this, i64* slot_id, i64* slot_size, u8** slot);
static i64 release(camio2_buffer_t* this, const i64 slot_id);
//cgeneric library, I've been meaning to reimplment to cgenerics stuff
//as it is a pretty ugly interface. Myabe this is the reason to do it..
static void cdelete(camio2_buffer_t* this2);


camio2_buffer_t* camio2_buffer_malloc_new(i64 slot_size, i64 slot_count){

    m6_log_debug1("Creating malloc based buffer with %li slots of %li bytes\n", slot_count, slot_size);

    //Precondidition checks
    if(unlikely(slot_size <= 0)){
        m6_log_warn("Slot size \"%li\" <= 0, this is probably not what you want\n", slot_size);
        return NULL;
    }
    if(unlikely(slot_count <= 0)){
        m6_log_warn("Slot count \"%li\" <= 0, this is probably not what you want\n", slot_count);
        return NULL;
    }

    //Do all the allocation and fail if it doesn't work out
    camio2_buffer_malloc_priv_t* result = calloc(1, sizeof(camio2_buffer_malloc_priv_t));
    if(unlikely(!result)){
        m6_log_error("Could not allocate camio2_buffer_malloc_priv_t. Failing now\n");
        return NULL;
    }

    //Page align these to minimise TLB and cache line crossings
    //TODO XXX: For portability this should be replaced with an appropriate call to getpagesize()
    m6_log_debug2("For portability this '4096' should be replaced with an appropriate call to getpagesize()");
    result->buffers = aligned_alloc(4096, slot_count * slot_size);
    bzero(result->buffers,  slot_count * slot_size );
    if(unlikely(!result->buffers)){
        free(result);
        m6_log_error("Could not allocate buffers. Failing now\n");
        return NULL;
    }

    //Page align these to minimise TLB and cache line crossings
    //TODO XXX: For portability this should be replaced with an appropriate call to getpagesize()
    m6_log_debug2("For portability this '4096' should be replaced with an appropriate call to getpagesize()");
    result->slots = aligned_alloc(4096, slot_count *sizeof(camio2_buffer_malloc_slot_t));
    bzero(result->slots, slot_count *sizeof(camio2_buffer_malloc_slot_t));
    if(unlikely(!result->slots)){
        free(result->buffers);
        free(result);
        m6_log_error("Could not allocate slots structure. Failing now\n");
        return NULL;
    }


    //Close the circle on the priv/this structure
    result->this.priv = result;

    result->this.reserve = reserve;
    result->this.release = release;
    result->this.cdelete = cdelete;


    result->slot_count = slot_count;
    result->slot_size  = slot_size;
    for(i64 slot = 0; slot < slot_count; slot++){
        result->slots[slot].slot_id = slot;
        result->slots[slot].buffer = result->buffers + slot * slot_size;
    }

    m6_log_debug1("Done creating malloc based buffer\n", slot_count, slot_size);
    return &result->this;
}


static void cdelete(camio2_buffer_t* this)
{
    m6_log_debug1("Deleting malloc based buffer...", slot_count, slot_size);
    camio2_buffer_malloc_priv_t* priv = (camio2_buffer_malloc_priv_t*)this->priv;
    free(priv->slots);
    free(priv->buffers);
    free(priv);
    m6_log_debug1("Done\n";)
}


static i64 reserve(camio2_buffer_t* this, i64* slot_id, i64* slot_size, u8** slot)
{
    if(unlikely(!slot_id)){
        m6_log_error("slot_id parameter is NULL, cannot continue\n");
        return CAMIO2_BUFFER_NULL_POINTER;
    }

    if(unlikely(!slot_size)){
        m6_log_error("slot_size parameter is NULL, cannot continue\n");
        return CAMIO2_BUFFER_NULL_POINTER;
    }

    if(unlikely(!slot)){
        m6_log_error("slot parameter is NULL, cannot continue\n");
        return CAMIO2_BUFFER_NULL_POINTER;
    }

    camio2_buffer_malloc_priv_t* priv = (camio2_buffer_malloc_priv_t*)this->priv;
    m6_log_debug2("This is slow and stupid. Searching for a free slot id in %li slots\n", priv->slot_count);
    for(int i = 0; i < priv->slot_count; i++){
        if(!priv->slots[i].reserved){
            m6_log_debug3("This is not thread safe!!\n");
            priv->slots[i].reserved = 1;
            *slot_id = i;
            *slot_size = priv->slot_size;
            *slot = priv->slots[i].buffer;
            m6_log_debug1("Found a free slot at slot id=%li\n", *slot_id);
            return CAMIO2_BUFFER_NO_ERROR;
        }
    }

    m6_log_debug1("No free slots found\n");
    return CAMIO2_BUFFER_NO_FREESLOTS;
}

static i64 release(camio2_buffer_t* this, const i64 slot_id)
{
    camio2_buffer_malloc_priv_t* priv = (camio2_buffer_malloc_priv_t*)this->priv;
    m6_log_debug1("Releasing slot id %li\n", slot_id);

    if(unlikely(slot_id < 0 || slot_id >= priv->slot_count)){
        m6_log_error("slot_id parameter is out of range. Valid range [%li,%li]\n", 0, priv->slot_count -1);
        return CAMIO2_BUFFER_OUT_OF_RANGE;
    }

    if(unlikely(!priv->slots[slot_id].reserved)){
        m6_log_warn("slot \"%li\" is already released. This is probably not what you want.\n", slot_id);
        return CAMIO2_BUFFER_INVALID_SLOT_ID;
    }

    priv->slots[slot_id].reserved = 0;
    return CAMIO2_BUFFER_NO_ERROR;
}

