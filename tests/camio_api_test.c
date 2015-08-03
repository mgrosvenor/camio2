/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 16, 2014
 *  File name: camio_api_test.c
 *  Description:
 *  Test the features of the CamIO API regardless of the channel used
 */

#include "../src/camio.h"
#include <stdio.h>
#include "../src/camio_debug.h"

USE_CAMIO;

int main(int argc, char** argv)
{

    (void)argc; //We don't use these for the test ... yet
    (void)argv;

    camio_device_t* device = NULL;

    new_camio_device_bin("netm",NULL,&device);

    DBG("Got new device at address %p\n", device);

    camio_channel_t* channel = NULL;
    camio_connect(device,&channel);

    camio_rd_buffer_t* rd_buff = NULL;
    ch_word rd_buff_len = 0;
    camio_read_acquire(channel,&rd_buff,&rd_buff_len,0, 0);

    /*
    camio_channel_t* channel = NULL;
    while(!camio_connect(device,&channel)){
        //Just spin waiting for a connection
    }

    camio_wr_buffer_t* wr_buffer_chain;
    ch_word count = 1;
    while(!camio_write_acquire(channel, &wr_buffer_chain, &count)){
        //Just spin waiting for a new write buffer
    }

    if(count != 1){
        //Shit, we should have got this!
    }


    while(1){

        camio_rd_buffer_t* rd_buffer_chain;
        while(camio_read_acquire(channel, &rd_buffer_chain, 0 , 0)){
            //Just spin waiting for a new read buffer
        }

        //Do a copy here

        camio_write_commit(channel, wr_buffer_chain, 0, 0 );

        camio_read_release(channel, rd_buffer_chain);
    }

    camio_write_release(channel,wr_buffer_chain);
    */



    return 0;
}
