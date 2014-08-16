/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 16, 2014
 *  File name: camio_api_test.c
 *  Description:
 *  Test the features of the CamIO API regardless of the stream used
 */

#include "../src/camio.h"
#include <stdio.h>

USE_CAMIO;

int main(int argc, char** argv)
{
    camio_t* camio = init_camio(); //Set up camio global state

    (void)camio; //Not using this right now

    (void)argc; //We don't use these for the test
    (void)argv;

    printf("Hello CamIO\n");
    return 0;
}
