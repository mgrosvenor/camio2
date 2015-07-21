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

USE_CAMIO;

int main(int argc, char** argv)
{

    (void)argc; //We don't use these for the test ... yet
    (void)argv;

    printf("Hello CamIO\n");

    camio_controller_t* connector = NULL;

    new_camio_transport_str("udp:127.0.0.1?rp=3000?wp=4000",NULL,&connector);

    printf("Got new connector at address %p\n", connector);

    camio_stream_t* stream = NULL;
    camio_connect(connector,&stream);

    while(!camio_connect(connector,&stream)){
        //Just spin waiting for a connection
    }

    printf("Got new stream at address %p\n", stream);

    /*
    camio_wr_buffer_t* wr_buffer_chain;
    ch_word count = 1;
    while(!camio_write_acquire(stream, &wr_buffer_chain, &count)){
        //Just spin waiting for a new write buffer
    }

    if(count != 1){
        //Shit, we should have got this!
    }
 */

   //while(1){

        camio_rd_buffer_t* rd_buffer_chain;
        ch_word chain_len = 1;
        while(camio_read_acquire(stream, &rd_buffer_chain, &chain_len , 0, 0)){
            //Just spin waiting for a new read buffer
        }

        printf("Got buffer with %lli bytes", rd_buffer_chain->data_len);

        //Do a copy here

        //camio_write_commit(stream, wr_buffer_chain, 0, 0 );

        //camio_read_release(stream, rd_buffer_chain);
    //}

    //camio_write_release(stream,wr_buffer_chain);



    return 0;
}
