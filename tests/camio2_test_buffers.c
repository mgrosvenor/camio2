// CamIO 2: camio2_test_buffers.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details. 
#include <stdio.h>
#include <alloca.h>
#include <unistd.h>

#include "../src/buffers/buffer.h"
#include "../src/buffers/buffer_malloc.h"

USE_M6_LOGGER(M6_LOG_LVL_DEBUG3,1,M6_LOG_OUT_STDERR,"");
USE_M6_OPTIONS;
USE_M6_PERF(1);




void test_camio2_buffer_malloc()
{

    printf("Running buffer_malloc tests..\n");
    m6_log_info("Running malloc buffer test 1...\n");
    camio2_buffer_t* test_buff1 = camio2_buffer_malloc_new(0,0);
    if(test_buff1) { m6_log_fatal("[Test 1 Failed] - No buffer expected from slot_size=0, slot_count = 0\n"); }

    m6_log_info("Running malloc buffer test 2...\n");
    camio2_buffer_t* test_buff2 = camio2_buffer_malloc_new(1,1);
    if(!test_buff2) { m6_log_fatal("[Test 2 Failed] - Buffer expected from slot_size=1, slot_count = 1\n"); }
    test_buff2->cdelete(test_buff2);

    m6_log_info("Running malloc buffer test 3...\n");
    camio2_buffer_t* test_buff3 = camio2_buffer_malloc_new(4096,1);
    if(!test_buff3) { m6_log_fatal("[Test 3 Failed] - Buffer expected from slot_size=4096, slot_count = 1\n"); }
    test_buff3->cdelete(test_buff3);

    m6_log_info("Running malloc buffer test 4...\n");
    camio2_buffer_t* test_buff4 = camio2_buffer_malloc_new(4096,4096);
    if(!test_buff4) { m6_log_fatal("[Test 4 Failed] - Buffer expected from slot_size=4096, slot_count = 4096\n"); }
    test_buff4->cdelete(test_buff4);

}


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    test_camio2_buffer_malloc();
    return 0;
}
