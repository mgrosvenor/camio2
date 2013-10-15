// CamIO 2: buffer.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef BUFFER_H_
#define BUFFER_H_

#include "../types/types.h"

//Slot information
typedef struct  {
    int valid;         //True if the data is valid (can be set to untrue by read_release)
    int data_len;     //Zero if there is no data. Length of data actually in the buffer
    int read_len;    //Length of data actually read.
    void* data_start; //Undefined if there is no data

    int buffer_len;      //Undefined if there is no data. Buffer_len is always >= data_len + (buffer_start - data_start)

    void* buffer_start; //Undefined if there is no data

    //Private - Don’t mess with these!
    int __buffer_id;  //Undefined if there is no data
    int __slot_id;   //Undefined if there is no data

    int __do_release;; //Should release be called for this slot?
    ciostr* __slot_parent; //Parent who generated this slot

} cioslot_info;


#endif /* BUFFER_H_ */
