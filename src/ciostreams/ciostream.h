// CamIO 2: ciostream.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 
#ifndef CIOSTREAM_H_
#define CIOSTREAM_H_

#include <stdbool.h>

#include "../types/types.h"
#include "../errors/errors.h"
#include "../buffers/buffer.h"
#include "../selectors/selector.h"
#include "../types/stdinclude.h"


//Stream features
typedef struct {
    bool is_reliable;       //Does this stream guarantee delivery
    bool is_encrypted;      //Is it encrypted
    bool has_async_arrv;    //Can messages arrive asynchronously
    bool is_bytestream;     //Do messages arrive as a stream of bytes or as a datagram
    bool can_read_off;      //Can the stream offset reads at zero cost
    int mtu;                //Maximum transfer unit for this stream
    int scope;              //1 = local this machine only.
                            //2 = L2 point to point connectivity only,
                            //3 = L3 Global connectivity (eg IP).
    bool is_thread_safe;    // Calls to functions from multiple threads will not cause threading problems.
} ciostr_inf;


/**
 * Request for stream features. Unless otherwise specified:
 * 1 = require stream to support feature,
 * 0 = require stream not to support feature,
 * -1 = don't care if stream supports feature
 */
//
typedef struct{
    int reliable;       //Does this stream guarantee delivery.
    int encrypted;      //Is it encrypted.
    int async_arrv;     //Can messages arrive asynchronously.
    int bytestream;     //Do messages arrive as a continuous stream of bytes, or as a datagram.
    int can_read_off;   //Can the stream offset reads into the receive buffer at zero cost.
    int mtu;            //Minimum value for maximum transfer unit for this stream,
    int scope;          //-1 = Don't care,
                        // 1 = at least local this machine only.
                        // 2 = at least L2 point to point connectivity only,
                        // 3 = L3 Global connectivity (eg IP).
    int thread_safe;    // Calls to functions from multiple threads will not cause threading problems.
} ciostr_req;


struct ciostrm_s {
    /**
     * Return the metadata structure describing the properties of this transport.
     */
    ciostr_inf (*get_info)(ciostr* this);

    /**
     * Return the the selectable structure for adding into a selector
     */
    cioselable* (*get_selectable)(ciostr* this);


    /**
     * This function tries to do a non-blocking read for new data from the CamIO Stream called “this” and return slot info
     * pointer called “slot_inf”. If the stream is empty, (e.g. end of file) or closed (e.g. disconnected) it is valid to return
     * 0 bytes.
     * Return values:
     * - ENOERROR:  Completed successfully, slot_info contains a valid structure.
     * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
     * - ENOSLOTS:  The stream could not allocate more slots for the read. Free some slots by releasing a read or write
     *              transaction.
     */
    int (*read_acquire)( ciostr* this, cioslot_info* slot_inf_o, int padding) ;


    /**
     * Try to acquire a slot for writing data into.  You can hang on to this slot as long as you like, but beware, most
     * streams offer a limited number of slots, and some streams offer only one. If you are using stream association for
     * zero-copy data movement, calling read_aquire has the effect as calling write_acquire.
     * Returns:
     *  - ENOERROR: Completed successfully, slot_info contains a valid structure.
     *  - ENOSLOTS: The stream could not allocate more slots for the read. Free some slots by releasing a read or write
     *              transaction.
     */
    int (*write_aquire)(ciostr* this, cioslot_info* slot_inf_o);


    /* Try to write data described by slot_info_i to the given stream called “this”.  If auto_release is set to true,
     * write_commit will release the resources associated with the slot when it is done. Some streams support bulk transfer
     * options to amortise the cost of system calls. For these streams, you can set enqueue to true. Release()
     * with enqueue set to 0 will flush the queue. Write_release() can be called with a NULL slot_inf_i parameter to flush
     * the queue. Unlike POSIX, write_comit() returns the number of bytes remaining rather than the number of bytes sent
     * to make it easy to wait until a write is committed.
     * Returns:
     * - ENOERROR: Completed successfully.
     * - EQFULL: The queue is full. Cannot enqueue more slots. Call write with enqueue set to 0.
     * - ECOPYOP: A copy operation was required to complete this commit.
     * - or: the bytes remaining for this stream. Since writing is non-blocking, this may not always be 0.
     */
    int (*write_commit)(ciostr* this, cioslot_info* slot_inf_i, int auto_release,  int enqueue);


    /**
     * Relinquish resources associated with the the slot. Some streams support asynchronous update modes. Release() will
     * check to see if the data in this slot is still valid. If it is not valid, it may have been trashed while you were
     * working on it, so results are invalid! If you are concerned about correctness, you should a) not use a stream
     * that supports asynchronous updates b) ensure that your software can “roll back” c) copy data and check for 
     * validity before proceeding. 
     * Return values:
     * - ENOERROR: All good, please continue
     * - EINVALID: Your data got trashed, time to recover!
     */
    int (*release)(ciostr* this, cioslot_info* slot_inf_i);

    /**
     * Free resources associated with this stream, but not with its connector. 
     */
    void (*destroy)(ciostr* this);

    /**
     * Pointer to stream specific data structures.
     */
    void* __priv;

};


struct cioconn_s {
    /**
     * Return the the selectable structure for adding into a selector
     */
    cioselable* (*get_selectable)(ciostr* this);


    /**
     * Non-blocking attempt to connect the underlying stream to it’s data source. If successful, the CamIO stream can be read
     *  and/or written to. In many cases, the connect operation will return immediately, with a valid stream once only.
     *  However, this is not guaranteed. Some streams may return multiple valid connection and some streams may take some
     *  time before they return successfully. Streams can be placed into selectors to facilitate the blocking behaviour
     *  necessary to wait for these events to happen by listening for the  on connection signal.
     *  Return values;
     *  - ENOERROR: Stream was created successfully.
     *  - ETRYAGAIN: The stream has nothing new to report at this time. No connection has yet happened.
     *  - ECHECKERROR: The stream creation has failed. 
     */
    int (*connect)( ciostr* this, struct ciostrm_s* ciostrm_o );

    /**
     * Free resources associated with this connector, but not with any of the streams it has created. 
     */
    void (*destroy)(ciostr* this);


    /**
     * Pointer to stream specific data structures.
     */
    void* __priv;
};


/**
 * Create a new CamIO connector with with given the URI and properties request. Return a pointer to connection object in 
 * cioconn_o. Cioconn_o is only valid id ENOERROR is returned. 
 * Returns:
 * - ENOERROR:  All good. Nothing to see here.
 * - EBADURI:   The URI supplied has a syntax error or is poorly formatted
 * - ENOSTREAM: The stream type is unrecognized
 * - EBADOPT:   The URI options have an error or are unsupported
 * - EBADPROP:  The stream supplied in URI did not match the properties requested.
 */
int new_cioconn( char* uri , ciostr_req* properties, struct cioconn_s** cioconn_o );


/**
 * This is a convenience method. Create a new CamIO connector and call connect(), blocking with the given selector and 
 * timeout until it succeeds or timesout. Free the connector by calling destroy() on it. Return a CamIO stream if 
 * successful. Returns only the first successful connection for streams that support many (e.g. TCP, Netnamp etc). 
 * Returns:
 * - ENOERROR:  All good. Nothing to see here.
 * - EBADURI:   The URI supplied has a syntax error or is poorly formatted
 * - ENOSTREAM: The stream type is unrecognized
 * - EBADOPT:   The URI options have an error or are unsupported
 * - EBADPROP:  The stream supplied in URI did not match the properties requested.
 * - ETRYAGAIN: The stream has nothing new to report at this time. No connection has yet happened.
 * - ECHECKERROR: The stream creation has failed. 
 */
int new_ciostrm( char* uri ,  ciostr_req* properties, ciosel selector, uint64_t timeout_ns, ciostr** ciostream_o );

#endif //CIOSTREAM_H_
