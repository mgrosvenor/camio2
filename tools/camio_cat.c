/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   May 2, 2015
 *  File name: camio_udp_test.c
 *  Description:
 *  Test the features of the CamIO API regardless of the stream used
 */

#include "../src/camio.h"
#include <stdio.h>
#include <src/api/api_easy.h>
#include <src/camio_debug.h>

USE_CAMIO;




int main(int argc, char** argv)
{

    (void)argc; //We don't use these for the test ... yet
    (void)argv;

    camio_stream_t* stream = NULL;
    camio_error_t err = camio_stream_new("udp:localhost:2000?wp=4000", &stream);
    if(err){
        DBG("Could not connect to stream\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }


   //Read and write a bytes to and from the stream
   while(1){
        camio_rd_buffer_t* rd_buffer = NULL;
        DBG("rd_buffer = %p &rd_buffer = %p\n", rd_buffer, &rd_buffer);
        err = CAMIO_ENOERROR;
        while( (err = camio_read_acquire(stream, &rd_buffer, 0, 0)) == CAMIO_ETRYAGAIN){
            //Just spin waiting for a new read buffer -- need to make a selector for this....
        }
        if(err){ DBG("Got a read error %i\n", err); return -1; }

        DBG("Got buffer with %lli bytes @buffer=%p, @data=%p\n",rd_buffer->data_len, rd_buffer->buffer_start, rd_buffer->data_start);

        //We got data, ok now acquire a write buffer to put stuff into it
        camio_rd_buffer_t* wr_buffer = NULL;
        while(camio_write_acquire(stream, &wr_buffer)){
            //Just spin waiting for a new write buffer -- need to make a selector for this....
        }
        DBG("Got write buffer with %lli bytes starting at %p\n", wr_buffer->buffer_len, wr_buffer->buffer_start);

        //TODO XXX this is temporary until the actual copy function is implemented
        DBG("Copying %lli bytes from %p to %p\n",rd_buffer->data_len,rd_buffer->buffer_start, wr_buffer->buffer_start);
        memcpy(wr_buffer->buffer_start,rd_buffer->data_start,rd_buffer->data_len);
        wr_buffer->data_start = wr_buffer->buffer_start;
        wr_buffer->data_len = rd_buffer->data_len;
        DBG("Done copying %lli bytes from %p to %p\n",rd_buffer->data_len,rd_buffer->buffer_start, wr_buffer->buffer_start);

        //Data copied, let's commit it and send it off
        camio_write_commit(stream, &wr_buffer );
        camio_write_release(stream,&wr_buffer);
        camio_read_release(stream, &rd_buffer);
    }

    return 0;
}
