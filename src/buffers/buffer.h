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
#include "../types/stdinclude.h"
#include "../types/types.h"

#define CIO_BUFFER_TST_NONE       0   //No timestamp on this stream
#define CIO_BUFFER_TST_S_1970     1   //Timestamp is seconds since 1970
#define CIO_BUFFER_TST_US_1970    2   //Timestamp is microseconds since 1970
#define CIO_BUFFER_TST_NS_1970    3   //Timestamp is nanoseconds since 1970
#define CIO_BUFFER_TST_TIMESPEC   4   //Timestamp is a timespec
#define CIO_BUFFER_TST_TIMEVAL    5   //Timestamp is a timeval
#define CIO_BUFFER_TST_FIXED3232  6   //Timestamp is a 32.32 fixed point value


//TODO XXX: Hide this from consumers by putting the definition in another file
typedef struct {
    bool __in_use;              //Is the buffer in use? If not, it can be reserved by someone
    uint64_t __pool_id;         //Undefined if there is no data
    uint64_t __buffer_id;       //Undefined if there is no data

    bool __do_release;          //Should release be called for this slot?
    ciostrm_t* __buffer_parent;   //Parent who generated this slot

    union {
        struct timespec ts_timespec;
        struct timeval  ts_timeval;
        uint64_t        ts_secs;
        uint64_t        ts_nanos;
        uint64_t        ts_micros;
        uint64_t        ts_fixed3232;
    } __ts; //Private, don't play with this directly!

    //Per stream private data goes here! (This must go last!)
    //**********************************************************************************************************************


} ciobuff_priv_t;



//Buffer information
typedef struct ciobuff_s {
    bool valid;          //True if the data is valid (can be set to untrue by read_release)
    int timestamp_type; //The type of timestamp associated with this stream

    //Some timestamps are inline with the data, some are not, this will point to the timestamp regardless of where it is.
    void* ts;

    uint64_t data_len;      //Zero if there is no data. Length of data actually in the buffer
    uint64_t orig_len;      //Sometimes there was more data than we have space. If orig_len > data_len, then
                            //truncation has happened. This probably only matters for reads
    void* data_start;       //Undefined if there is no data. Data may be offset into the buffer to allow for headers etc.

    uint64_t buffer_len;    //Undefined if there is no data. Buffer_len is always >= data_len + (buffer_start - data_start)
    void* buffer_start;     //Undefined if there is no data

    struct ciobuff_s* next; //Pointer to the next buffer in this queue, if null, there is no more.

    //Private - "Consumers" should not mess with these! (This must go last!)
    //**********************************************************************************************************************
    ciobuff_priv_t __priv;

} ciobuff_t;

typedef ciobuff_t ciobuff_rd_t; //We make these incompatible so that the type checker will help us. The only way to get
typedef ciobuff_t ciobuff_wr_t; //from a read (rd) buffer to a write (wr) buffer is to do a buffer copy. Sometimes a real
                                //copy will happen as a result.

#endif /* BUFFER_H_ */
