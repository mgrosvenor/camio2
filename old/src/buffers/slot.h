// CamIO 2: slot.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef SLOT_H_
#define SLOT_H_

#include <time.h>
#include <sys/time.h>
#include "../types/stdinclude.h"
#include "../types/types.h"


//Slot information
typedef struct  {
    bool valid;          //True if the data is valid (can be set to untrue by read_release)
    int timestamp_type; //The type of timestamp associated with this channel
    #define CIO_SLOT_TST_NONE       0   //No timestamp on this channel
    #define CIO_SLOT_TST_S_1970     1   //Timestamp is seconds since 1970
    #define CIO_SLOT_TST_US_1970    2   //Timestamp is microseconds since 1970
    #define CIO_SLOT_TST_NS_1970    3   //Timestamp is nanoseconds since 1970
    #define CIO_SLOT_TST_TIMESPEC   4   //Timestamp is a timespec
    #define CIO_SLOT_TST_TIMEVAL    5   //Timestamp is a timeval
    #define CIO_SLOT_TST_FIXED3232  6   //Timestamp is a 32.32 fixed point value
    union {
        struct timespec ts_timespec;
        struct timeval  ts_timeval;
        uint64_t        ts_secs;
        uint64_t        ts_nanos;
        uint64_t        ts_micros;
        uint64_t        ts_fixed3232;
    } __ts;

    //Some timestamps are inline with the data, some are not, this will point to the timestamp regardless of where it is.
    void* ts;

    uint64_t data_len;      //Zero if there is no data. Length of data actually in the buffer
    uint64_t read_len;      //Sometimes a read has more data than we have space. If read_len > data_len, then
                            //truncation has happened.
    void* data_start;       //Undefined if there is no data. Data may be offset into the buffer.

    uint64_t slot_len;    //Undefined if there is no data. Buffer_len is always >= data_len + (buffer_start - data_start)

    void* slot_start;     //Undefined if there is no data

    //Private - "Consumers" should not mess with these!
    //**********************************************************************************************************************
    bool __in_use;			//Is the buffer in use? If not, it can be reserved by someone
    uint64_t __buffer_id;   //Undefined if there is no data
    uint64_t __slot_id;     //Undefined if there is no data

    bool __do_release;      //Should release be called for this slot?
    ciostrm* __slot_parent;  //Parent who generated this slot

    void* __priv;             //Per channel private data if necessary

} cioslot;


#endif /* SLOT_H_ */
