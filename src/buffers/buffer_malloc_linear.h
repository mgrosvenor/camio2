/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    May 13, 2015
 *  File name:  buffer_malloc_linear.h
 *  Description:
 *  A linear search allocating/deallocating malloc based CamIO buffer
 */
#ifndef BUFFER_MALLOC_LINEAR_H_
#define BUFFER_MALLOC_LINEAR_H_
#include <src/buffers/buffer.h>

typedef struct buffer_malloc_linear_s buffer_malloc_linear_t;


camio_error_t buffer_malloc_linear_new(camio_stream_t* parent, ch_word buffer_size, ch_word bufffer_count,
        ch_bool readonly, buffer_malloc_linear_t** lin_buff_o);


camio_error_t buffer_malloc_linear_acquire(buffer_malloc_linear_t* lin_buff, camio_buffer_t** buffer_o);
camio_error_t buffer_malloc_linear_release(buffer_malloc_linear_t* lin_buff, camio_buffer_t** buffer_o);

void buffer_malloc_linear_destroy(buffer_malloc_linear_t** lin_buff_io);

#endif /* BUFFER_MALLOC_LINEAR_H_ */
