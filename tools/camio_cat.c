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

    //Create a new multiplexer for streams to go into
    camio_mux_t* mux = NULL;
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);

    //Create and connect to a new stream
    camio_stream_t* stream = NULL;
    err = camio_stream_new("tcp:localhost:2000?listen=1", &stream);
    if(err){ DBG("Could not connect to stream\n"); return CAMIO_EINVALID; /*TODO XXX put a better error here*/ }

    //Put the read stream into the mux
    camio_mux_insert(mux,&stream->rd_muxable,0);

   //Read and write bytes to and from the stream - just do loopback for now
    camio_rd_buffer_t* rd_buffer = NULL;
    camio_rd_buffer_t* wr_buffer = NULL;
    camio_muxable_t* which       = NULL;
    while(1){
        //Tell the stream that we want some data
        camio_read_request(stream,0,0);

        //Block waiting for a stream to be ready
        camio_mux_select(mux,&which);

        //Acquire a pointer to the new data now that it's ready
        err = camio_read_acquire(which->parent.stream, &rd_buffer);
        if(err){ DBG("Got a read error %i\n", err); return -1; }

        if(rd_buffer->data_len == 0){
            break; //The connection is dead!
        }

        //Get a buffer for writing stuff to
        err = camio_write_acquire(stream, &wr_buffer);
        if(err){ DBG("Could not connect to stream\n"); return CAMIO_EINVALID; /*TODO XXX put a better error here*/ }

        //Copy data over from the read buffer to the write buffer
        err = camio_copy_rw(&wr_buffer,&rd_buffer,0,rd_buffer->data_len);
        if(err){ DBG("Got a copy error %i\n", err); return -1; }

        //Data copied, let's commit it and send it off
        err = camio_write_commit(stream, &wr_buffer );
        if(err){ DBG("Got a commit error %i\n", err); return -1; }

        //Done with the write buffer, release it
        camio_write_release(stream,&wr_buffer);

        //And we're done with the read buffer too, release it as well
        err = camio_read_release(stream, &rd_buffer);
        if(err){ DBG("Got a read release error %i\n", err); return -1; }
    }

    return 0;
}
