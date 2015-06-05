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

    new_camio_transport_str("udp:127.0.0.1?rp=3000?wp=4000",NULL,&connector);
    DBG("## Got new connector at address %p\n", connector);

    camio_stream_t* stream = NULL;
    while(camio_connect(connector,&stream)){
        //Just spin waiting for a connection -- need to make a selector for this....
    }
    DBG("## Got new stream at address %p\n", stream);

   //while(1){

        camio_rd_buffer_t* rd_buffer_chain = NULL;
        DBG("buffer_chain = %p &buffer_chain = %p\n", rd_buffer_chain, &rd_buffer_chain);
        ch_word rd_chain_len = 1;
        camio_error_t err = 0;
        while( (err = camio_read_acquire(stream, &rd_buffer_chain, &rd_chain_len , 0, 0)) == CAMIO_ETRYAGAIN){
            //Just spin waiting for a new read buffer -- need to make a selector for this....
            //DBG("buffer_chain2 = %p &buffer_chain = %p\n", rd_buffer_chain, &rd_buffer_chain);
        }
        if(err){
            DBG("Got a read error %i\n", err);
            return -1;
        }
        DBG("Got %lli read%s buffer with %lli bytes\n", rd_chain_len, rd_chain_len != 1 ? "s" : "",  rd_buffer_chain->data_len);


        camio_rd_buffer_t* wr_buffer_chain = NULL;
        ch_word wr_chain_len = 1;
        while(camio_write_acquire(stream, &wr_buffer_chain, &wr_chain_len)){
            //Just spin waiting for a new write buffer -- need to make a selector for this....
        }
        DBG("Got write buffer with %lli bytes\n", rd_buffer_chain->buffer_len);

        DBG("Copying %lli bytes from %p to %p\n",rd_buffer_chain->data_len,wr_buffer_chain->buffer_start, rd_buffer_chain->buffer_start);
        memcpy(wr_buffer_chain->data_start,rd_buffer_chain->data_start,rd_buffer_chain->data_len);
        wr_buffer_chain->data_len = rd_buffer_chain->data_len;

        camio_write_commit(stream, &wr_buffer_chain, 0, 0 );
        camio_write_release(stream,&wr_buffer_chain);
        camio_read_release(stream, &rd_buffer_chain);
    //}

    return 0;
}
