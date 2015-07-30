/*
 * CamIO - The Cambridge Input/Output API
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details.
 *
 *  Created:   May 2, 2015
 *  File name: camio_udp_test.c
 *  Description:
 *  Test the features of the CamIO API regardless of the channel used
 */

#include <stdio.h>
#include <src/api/api_easy.h>
#include <deps/chaste/chaste.h>

#include "camio_perf_client.h"
#include "camio_perf_server.h"
#include "options.h"

USE_CAMIO;
USE_CH_OPTIONS;
USE_CH_LOGGER_DEFAULT;


struct options_t options;


int main(int argc, char** argv)
{
    #ifdef NDEBUG
    printf("NDEBUG defined\n");
    #endif

    DBG("Starting camio perf!\n");
    ch_opt_addsi(CH_OPTION_OPTIONAL, 'c', "client","name or that the client should connect to", &options.client, NULL);
    ch_opt_addsi(CH_OPTION_OPTIONAL, 's', "server","put camio_perf in server mode", &options.server, NULL);
    ch_opt_addii(CH_OPTION_OPTIONAL, 'l', "len","max length of read/write buffer. This may be smaller\n", &options.len, 8 * 1024);
    ch_opt_addii(CH_OPTION_OPTIONAL, 'b', "batching","Amount of batching to apply. This will affect the latency vs. throughput\n", &options.batching, 8);
    ch_opt_parse(argc,argv);

    if(options.client && options.server){
        ch_log_fatal("Cannot be in both client and server mode at the same time, please choose one!\n");
    }

    ch_word stop = 0;
    if(options.client){
        DBG("Starting camio perf client\n");
        camio_perf_clinet(options.client,&stop);
    }
    else{
        DBG("Starting camio perf server\n");
        camio_perf_server(options.server,&stop);

    }

    return 0;
}
