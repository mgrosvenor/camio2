/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jul 1, 2015
 *  File name:  buffer.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "buffer.h"

//Reset the buffers internal pointers to point to nothing
void reset_buffer(camio_buffer_t* dst)
{
    dst->__internal.__mem_start = NULL;
    dst->__internal.__mem_len   = 0;
    dst->data_start             = NULL;
    dst->data_len               = 0;
}


////Assign the pointers from one buffer to another
//void assign_buffer2(camio_buffer_t* dst, camio_buffer_t* src, void* data_start, ch_word data_len)
//{
//    dst->__internal.__mem_start = src->__internal.__mem_start;
//    dst->__internal.__mem_len   = src->__internal.__mem_len;
//    dst->data_start             = data_start;
//    dst->data_len               = data_len;
//}

//Take a source buffer, and slice out a chunk content from it
void buffer_slice(camio_buffer_t* dst, camio_buffer_t* src, void* content_start, ch_word content_len)
{
    dst->__internal.__mem_start = src->data_start;
    dst->__internal.__mem_len   = src->data_len;
    dst->data_start             = content_start;
    dst->data_len               = content_len;
}
