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

typedef struct {
    char* str;
    m6_bool is_const;
    i64 slen;
    i64 mlen;
} m6_str;

#define M6_STR_CONST_NEW(name, s) m6_str name = M6_STR_CONST_LIT(s)
#define M6_STR_CONST_LIT(s) { .str = s, .is_const = 1, .slen = strlen(s), .mlen = strlen(s) }
#define M6_STR_CONST_FUNC(s) { .str = s, is_const = -1, .slen = -1, }

//Make a new m6_string on the stack, no free is necessary when done with it, but don,t pass it up a function call
m6_str m6_str_new_stack_s(char* s,i64 size){
    m6_str result;
    result.mlen = MIN(size + 1,getpagesize()); //Put a sensible bound on the size of these things
    result.slen = strnlen(s,size);
    if(unlikely(result.slen > result.mlen - 1)){
        result.slen = size - 1;
    }

    result.str = (char*)alloca(result.mlen);
    result.str[result.mlen -1] = '\0'; //Always null terminate the whole thing
    memcpy(result.str,s,result.slen);//Also null terminate the actual string.
    result.str[result.slen] = '\0';
    result.is_const = 1;

    return result;
}

//Make a new m6_string on the heap, you need to free it when done with it
m6_str m6_str_new_heap_s(char* s,i64 size){
    m6_str result;
    result.mlen = MIN(size + 1,getpagesize()); //Put a sensible bound on the size of these things
    result.slen = strnlen(s,size);
    if(unlikely(result.slen > result.mlen - 1)){
        result.slen = size - 1;
    }

    result.str = (char*)malloc(result.mlen);
    result.str[result.mlen -1] = '\0'; //Always null terminate the whole thing
    memcpy(result.str,s,result.slen);
    result.str[result.slen] = '\0';//Aslo null terminate the actual string
    result.is_const = 0;

    return result;
}

//Concatenate two strings and return a string on the stack (no need to free it)
m6_str m6_str_cat_stack(m6_str lhs, m6_str rhs){

}



void test_camio2_buffer_malloc()
{

    M6_STR_CONST_NEW(test,"");

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
