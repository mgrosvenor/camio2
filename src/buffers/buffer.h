/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 2014
 *  File name: buffer.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef BUFFER_H_
#define BUFFER_H_

#include <time.h>
#include <sys/time.h>
#include <src/types/types.h>

typedef enum camio_buffer_timestamp_e {
    CAMIO_BUFFER_TST_NONE = 0,      //No timestamp on this stream
    CAMIO_BUFFER_TST_S_1970,        //Timestamp is seconds since 1970
    CAMIO_BUFFER_TST_US_1970,       //Timestamp is microseconds since 1970
    CAMIO_BUFFER_TST_NS_1970,       //Timestamp is nanoseconds since 1970
    CAMIO_BUFFER_TST_TIMESPEC,      //Timestamp is a timespec
    CAMIO_BUFFER_TST_TIMEVAL,       //Timestamp is a timeval
    CAMIO_BUFFER_TST_FIXED3232,     //Timestamp is a 32.32 fixed point value
} camio_buffer_timestamp_t;


//TODO XXX: Hide this from consumers by putting the definition in another file
typedef struct camio_buffer_internal_s {
    ch_bool __in_use;                   //Is the buffer in use? If not, it can be reserved by someone
    ch_bool __read_only;                //For read buffers to protect against copying. Not bullet proof, but no harm
    ch_word __pool_id;                 //Undefined if there is no data
    ch_word __buffer_id;               //Undefined if there is no data

    bool __do_auto_release;             //Should release be called automatically for this slot?
    camio_stream_t* __parent;    //Parent who stream from who owned this slot.

    union {
        struct timespec ts_timespec;
        struct timeval  ts_timeval;
        uint64_t        ts_secs;
        uint64_t        ts_nanos;
        uint64_t        ts_micros;
        uint64_t        ts_fixed3232;
    } __ts; //Private, don't play with this directly!

    int64_t __mem_len;    //Undefined if there is no data. Size of the underlying memory
    void* __mem_start;    //Undefined if there is no data. Pointer to the underlying memory

} camio_buffer_internal_t;


//Place holder for functions
//typedef struct camio_buffer_interface_s{
//} camio_buffer_interface_t;




//Buffer information
typedef struct camio_buffer_s {
    //camio_buffer_interface_t vtable; //Place holder for functions


    bool valid;          //True if the data is valid (can be set to untrue by read_release)
    camio_buffer_timestamp_t timestamp_type; //The type of timestamp associated with this stream

    //Some timestamps are inline with the data, some are not, this will point to the timestamp regardless of where it is.
    void* ts;

    int64_t data_len;      //Zero if there is no data. Length of data actually in the buffer
    int64_t orig_len;      //Sometimes there was more data than we have space. If orig_len > data_len, then                            //truncation has happened. This probably only matters for reads
    void* data_start;       //Undefined if there is no data. Data may be offset into the buffer to allow for headers etc.

    camio_buffer_internal_t __internal; //Internal state, do not touch unless you are are stream implementor

} camio_buffer_t;

typedef camio_buffer_t camio_rd_buffer_t; //We make these incompatible so that the type checker will help us. The only way to
typedef camio_buffer_t camio_wr_buffer_t; //get from a read (rd) buffer to a write (wr) buffer is to do a buffer copy.
                                          //Sometimes a real copy will happen as a result.


/**
 * Some macros to make life easier BUFFER_GET_PRIVATE helps us grab the private members out of the public connector_t and
 * CONNECTOR_DECLARE help to make a custom allocator for each stream
 */
#define BUFFER_GET_PRIVATE(THIS) ( (void*)((THIS) + 1))

#define CALLOC_BUFFER(name, number)\
        calloc_##name##_buffer(number)

#define NEW_BUFFER(name)\
        calloc_##name##_buffer(1)

#define CALLOC_BUFFER_DECLARE(NAME)\
        camio_buffer_t* calloc_##NAME##_buffer(ch_word number)

#define CALLOC_BUFFER_DEFINE(NAME, PRIVATE_TYPE) \
    CALLOC_BUFFER_DECLARE(NAME)\
    {\
        camio_buffer_t* result = (camio_buffer_t*)calloc(number,sizeof(camio_buffer_t) + sizeof(PRIVATE_TYPE));\
        if(!result) return NULL;\
        return result;\
    }


//void assign_buffer(camio_buffer_t* dst, camio_buffer_t* src, void* data_start, ch_word data_len);
void buffer_slice(camio_buffer_t* dst, camio_buffer_t* src, void* content_start, ch_word content_len);
void reset_buffer(camio_buffer_t* dst);

#endif /* BUFFER_H_ */
