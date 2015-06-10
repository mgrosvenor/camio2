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

#include <src/camio_debug.h>

USE_CAMIO;

int main(int argc, char** argv)
{

    (void)argc; //We don't use these for the test ... yet
    (void)argv;

    camio_connector_t* connector = NULL;

    //Make and populate a parameters structure
    void* params;
    ch_word params_size;
    ch_word id;
    camio_transport_params_new("udp:127.0.0.1?rd_address=localhost?rd_protocol=3000?wr_address=localhost?wr_protocol=4000",&params, &params_size, &id);

    exit(1);
    //Use the parameters structure to construct a new connector object
    camio_transport_constr(id,&params,params_size,&connector);

    DBG("## Got new connector at address %p\n", connector);

    camio_stream_t* stream = NULL;
    while(camio_connect(connector,&stream)){
        //Just spin waiting for a connection -- need to make a selector for this....
    }
    DBG("## Got new stream at address %p\n", stream);

   //while(1){

        camio_rd_buffer_t* rd_buffer = NULL;
        DBG("buffer_chain = %p &buffer_chain = %p\n", rd_buffer, &rd_buffer);
        camio_error_t err = 0;
        while( (err = camio_read_acquire(stream, &rd_buffer, 0, 0)) == CAMIO_ETRYAGAIN){
            //Just spin waiting for a new read buffer -- need to make a selector for this....
            //DBG("buffer_chain2 = %p &buffer_chain = %p\n", rd_buffer_chain, &rd_buffer_chain);
        }
        if(err){
            DBG("Got a read error %i\n", err);
            return -1;
        }
        DBG("Got buffer with %lli bytes\n",rd_buffer->data_len);


        camio_rd_buffer_t* wr_buffer = NULL;
        while(camio_write_acquire(stream, &wr_buffer)){
            //Just spin waiting for a new write buffer -- need to make a selector for this....
        }
        DBG("Got write buffer with %lli bytes\n", rd_buffer->buffer_len);

        DBG("Copying %lli bytes from %p to %p\n",rd_buffer->data_len,wr_buffer->buffer_start, rd_buffer->buffer_start);
        //TODO XXX this is temporary until the actual copy function is implemented
        memcpy(wr_buffer->data_start,rd_buffer->data_start,rd_buffer->data_len);
        wr_buffer->data_len = rd_buffer->data_len;

        camio_write_commit(stream, &wr_buffer );
        camio_write_release(stream,&wr_buffer);
        camio_read_release(stream, &rd_buffer);
    //}

    return 0;
}
