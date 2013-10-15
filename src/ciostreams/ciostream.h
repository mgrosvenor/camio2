// CamIO 2: ciostream.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 
#ifndef CIOSTREAM_H_
#define CIOSTREAM_H_

#include "../types/types.h"

//Stream information
typedef struct {
    int reliable;       //Does this stream guarantee delivery
    int encrypted;  //Is it encrypted
    int async_arrv;     //Can messages arrive asynchronously
    int mtu;        //Maximum transfer unit for this stream
    int can_read_off;   //Can the stream offset reads at zero cost
    int scope;      //1 = local this machine only. 2 = L2 point to point connectivity only, 3 = L3 Global connectivity (eg IP).
    int thread_safe;    // Calls to functions from multiple threads will not cause threading problems.
} stream_inf;

typedef struct{
    int reliable;       //Does this stream guarantee delivery
    int encrypted;  //Is it encrypted
    int async_arrv;     //Can messages arrive asynchronously
    int mtu;        //Maximum transfer unit for this stream
    int can_read_off;   //Can the stream offset reads at zero cost
    int scope;      //1 = local this machine only. 2 = L2 point to point connectivity only, 3 = L3 Global connectivity (eg IP).
    int thread_safe;    // Calls to functions from multiple threads will not cause threading problems.
} stream_req;



struct ciostr_s {
    //Return the metadata structure describing the properties of this transport.
    stream_inf (*info)(ciostr* this);


    /*
     * This function tries to do a non-blocking read for new data from the CamIO Stream called “this” and return slot info
     * pointer called “slot_inf”. If the stream is empty, (e.g. end of file) or closed (e.g. disconnected) it is valid to return
     * 0 bytes.  Return values:
     * - ENOERROR:  Completed successfully, slot_info contains a valid structure.
     * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
     * - ENOSLOTS:  The stream could not allocate more slots for the read. Free some slots by releasing a read or write
     *              transaction.
     */
    int read_acquire( ciostr* this, cioslot_info* slot_inf_o, int padding) ;

    /**
     * Try to acquire a slot for writing data into.  You can hang on to this slot as long as you like, but beware, most
     * streams offer a limited number of slots, and some streams offer only one. If you are using stream association for
     * zero-copy data movement, calling read_aquire has the effect of calling write_acquire. Returns:
     *  - ENOERROR: Completed successfully, slot_info contains a valid structure.
     *  - ENOSLOTS: The stream could not allocate more slots for the read. Free some slots by releasing a read or write
     *              transaction.
     */
    int write_aquire(ciostr* this, cioslot_info* slot_inf_o);

    /* Try to write data described by slot_info_i to the given stream called “this”.  If auto_release is set to true,
     * write_commit will release the resources associated with the slot when it is done. Some streams support bulk transfer
     * options to amortize the cost of system calls. For these streams, you can set enqueue to true. Calling write_release()
     * with enqueue set to 0 will flush the queue. Write_release() can be called with a NULL slot_inf_i parameter to flush
     * the queue. Unlike POSIX, write_comit() returns the number of bytes remaining to be sent in bytes_remaining. Returns:
     * - ENOERROR: Completed successfully.
     * - EQFULL: The queue is full. Cannot enqueue more slots. Call write with enqueue set to 0.
     * - ECOPYOP: A copy operation was required to complete this commit.
     */
    int write_commit(ciostr* this, cioslot_info* slot_inf_i, int auto_release,  int enqueue, int* bytes_remainig);

    /**
     * Relinquish resources associated with the the slot. Some streams support asynchronous update modes. Release() will
     * check to see if the data in this slot is still valid. If it is not valid, it may have been trashed will you were
     * working on it, so results are invalid! Return values:
     * - NOERROR: All good, please continue
     * - EINVALID: Your data got trashed, time to recover!
     */
    int release(ciostr* this, slot_inf* slot_inf_i);

    void* __priv;

};


// Create a new CamIO stream with with given uri.
ciostr* new_ciostr( char* uri ,  stream_* properties );


#endif /* CIOSTREAM_H_ */