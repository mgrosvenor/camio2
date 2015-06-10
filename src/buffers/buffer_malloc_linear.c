/*
 * buffer_malloc_linear.c
 *
 *  Created on: May 13, 2015
 *      Author: mgrosvenor
 */
#include "buffer_malloc_linear.h"
#include "../camio_debug.h"

typedef struct buffer_malloc_linear_s {
    ch_word buffer_size;
    ch_word buffer_count;

    char** buffers;
    camio_buffer_t* headers;

    ch_word alloc_tail_idx; //The index of the first in-use buffer
    ch_word alloc_count;    //The number of in use buffers

} buffer_malloc_linear_t;


void buffer_malloc_linear_DBG(buffer_malloc_linear_t* buff){
    DBG("buffer_size=%li\n",buff->buffer_size);
    DBG("buffer_count=%li\n",buff->buffer_count);
    DBG("HDRS: ");
    for(int i = 0; i < buff->buffer_count; i++){
        DBG2("%i",buff->headers[i].__internal.__in_use);
    }
    DBG2("\n");
}

static void init_buffer_hdr(camio_stream_t* parent, camio_buffer_t* buffer_hdr, char* buffer_data, ch_word buffer_size,
        ch_word buffer_id, ch_bool readonly){
    buffer_hdr->buffer_len                     = buffer_size;
    buffer_hdr->buffer_start                   = buffer_data;
    buffer_hdr->__internal.__buffer_id         = buffer_id;
    buffer_hdr->__internal.__buffer_parent     = parent;
    buffer_hdr->__internal.__do_auto_release   = false;
    buffer_hdr->__internal.__in_use            = false;
    buffer_hdr->__internal.__pool_id           = 0;
    buffer_hdr->__internal.__read_only         = readonly;
}


void buffer_malloc_linear_destroy(buffer_malloc_linear_t** lin_buff_io)
{
    if(!lin_buff_io){ return; } //Asked me to free with a null pointer pointer? What?
    if(!*lin_buff_io){ return; } //Asked me to free a null structure? What?
    buffer_malloc_linear_t* lin_buff = *lin_buff_io; //It's now safe to do this

    //Free buffer pointer array if allocated
    if(lin_buff->buffers){

        //Free each buffer if allocated
        for(ch_word i = 0; i < lin_buff->buffer_count; i++){
            if(lin_buff->buffers[i]){
                free(lin_buff->buffers[i]);
                lin_buff->buffers[i] = NULL;
            }
        }
        free(lin_buff->buffers);
    }

    if(lin_buff->headers){
        free(lin_buff->headers);
    }

    //Free the linear buffer structure
    free(*lin_buff_io);
    *lin_buff_io = NULL; //Set the users pointer null, should avoid some dangling pointers
}


camio_error_t buffer_malloc_linear_new(camio_stream_t* parent, ch_word buffer_size, ch_word buffer_count,
        ch_bool readonly, buffer_malloc_linear_t** lin_buff_o)
{
    if(!lin_buff_o){
        return CAMIO_EINVALID; //What? Why give me a null pointer to work with??
    }

    buffer_malloc_linear_t* result = calloc(1,sizeof(buffer_malloc_linear_t));
    if(!result){
        buffer_malloc_linear_destroy(&result);
        return CAMIO_ENOMEM;
    }

    result->buffers = calloc(buffer_count, sizeof(char*));
    if(!result->buffers){
        buffer_malloc_linear_destroy(&result);
        return CAMIO_ENOMEM;
    }

    result->headers  = calloc(buffer_count, sizeof(camio_buffer_t));
    if(!result->headers){
        buffer_malloc_linear_destroy(&result);
        return CAMIO_ENOMEM;
    }

    result->buffer_count = buffer_count;
    result->buffer_size = buffer_size;

    //Alloc and init the actual memory buffers
    for(ch_word i = 0; i < buffer_count; i++){

        result->buffers[i] = calloc(1, buffer_size);
        if(!result->buffers[i]){
            buffer_malloc_linear_destroy(&result);
            return CAMIO_ENOMEM;
        }

        init_buffer_hdr(parent,&result->headers[i], result->buffers[i],buffer_size,i,readonly);
    }

    //Yay! We're done!!
    *lin_buff_o = result; //Finally we're done, pass out the successful allocation
    //buffer_malloc_linear_DBG(*lin_buff_o);
    return CAMIO_ENOERROR;
}


camio_error_t buffer_malloc_linear_acquire(buffer_malloc_linear_t* lin_buff, camio_buffer_t** buffer_o)
{
    const ch_word alloc_tail_idx    = lin_buff->alloc_tail_idx;
    const ch_word alloc_count       = lin_buff->alloc_count;
    const ch_word buffer_count      = lin_buff->buffer_count;

    if(alloc_count >= buffer_count){
        //We've run out jim. Now what the hell do we do??
        return CAMIO_ENOBUFFS;
    }

    const ch_word alloc_head_idx = (alloc_tail_idx + alloc_count) % buffer_count;
    if(lin_buff->headers[alloc_head_idx].__internal.__in_use){
        //Probably some internal logic flaw if this happens!
        return CAMIO_EBUFFINUSE;
    }

    if(lin_buff->headers[alloc_head_idx].__internal.__buffer_id != alloc_head_idx){
        //Probably some internal logic flaw if this happens!
        return CAMIO_EBUFFERROR;
    }

    lin_buff->headers[alloc_head_idx].__internal.__in_use = true;
    lin_buff->alloc_count++;
    *buffer_o = &lin_buff->headers[alloc_head_idx];
    //buffer_malloc_linear_DBG(lin_buff);
    return CAMIO_ENOERROR;
}


camio_error_t buffer_malloc_linear_release(buffer_malloc_linear_t* lin_buff, camio_buffer_t** buffer_o)
{
    const ch_word alloc_tail_idx = lin_buff->alloc_tail_idx;

    if((*buffer_o)->__internal.__buffer_id != alloc_tail_idx){
        //You can't free this buffer!
        return CAMIO_EWRONGBUFF;
    }
    lin_buff->headers[alloc_tail_idx].__internal.__in_use = false;
    lin_buff->alloc_count--;
    lin_buff->alloc_tail_idx = (alloc_tail_idx + 1) % lin_buff->buffer_count;
   // buffer_malloc_linear_DBG(lin_buff);
    return CAMIO_ENOERROR;
}


