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

#include <stdio.h>
#include <src/api/api_easy.h>
#include <src/camio_debug.h>

USE_CAMIO;

int main(int argc, char** argv)
{

    //We don't use these for the test ... yet
    (void)argc;
    (void)argv;

    //Create and connect to a new stream
    camio_stream_t* stream = NULL;
    camio_error_t err = camio_stream_new("udp:localhost:2000?wp=4000", &stream);
    if(err){ DBG("Could not connect to stream\n"); return CAMIO_EINVALID; /*TODO XXX put a better error here*/ }

    //Get a buffer for writing stuff
    camio_rd_buffer_t* wr_buffer = NULL;
    err = camio_write_acquire(stream, &wr_buffer);
    if(err){ DBG("Could not connect to stream\n"); return CAMIO_EINVALID; /*TODO XXX put a better error here*/ }

   //Read and write bytes to and from the stream - just do loopback for now
    camio_rd_buffer_t* rd_buffer = NULL;
    while(1){
        while( (err = camio_read_acquire(stream, &rd_buffer)) == CAMIO_ETRYAGAIN){
            //Just spin waiting for a new read buffer -- TODO XXX need to make a selector for this....
        }
        if(err){ DBG("Got a read error %i\n", err); return -1; }

        //Got some new data on the read stream, copy it over to the write stream
        err = camio_copy_rw(&wr_buffer,&rd_buffer,0,rd_buffer->data_len);
        if(err){ DBG("Got a copy error %i\n", err); return -1; }

        //Data copied, let's commit it and send it off
        err = camio_write_commit(stream, &wr_buffer );
        if(err){ DBG("Got a commit error %i\n", err); return -1; }

        //And we're done with that buffer
        err = camio_read_release(stream, &rd_buffer);
        if(err){ DBG("Got a read release error %i\n", err); return -1; }
    }

    //Done with the write buffer too
    camio_write_release(stream,&wr_buffer);

    return 0;
}
