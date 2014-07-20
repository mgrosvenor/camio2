/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 2014
 *  File name: stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef STREAM_H_
#define STREAM_H_

#include "../types/types.h"

#define CIOSTRM_TX_QUEUE_MAX 256


/**
 * Request (req) for stream features. Unless otherwise specified:
 * 1 = require stream to support feature,
 * 0 = require stream not to support feature,
 * -1 = don't care if stream supports feature
 *
 * Result (res) from stream feature request. Unless otherwise specified:
 * 1 = stream supports feature
 * 0 = stream does not support feature
 */
//
typedef struct{
    int reliable;       // req/res: Does this stream guarantee delivery.
    int encrypted;      // req/res: Is it encrypted.
    int async_arrv;     // req/res: Can messages arrive asynchronously.
    int bytestream;     // req/res: Do messages arrive as a continuous stream of bytes, or as a datagram.
    int can_read_off;   // req/res: Can the stream offset reads into the receive buffer at zero cost.
    int mtu;            // req: Minimum value for maximum transfer unit for this stream,
                        // res: Maximum transfer unit for this stream.
    int scope;          // req/res:
                        // -1 = Don't care,
                        // 1 = at least local this machine only.
                        // 2 = at least L2 point to point connectivity only,
                        // 3 = L3 Global connectivity (eg IP).
    int thread_safe;    // req/res: Calls to functions from multiple threads will not cause threading problems.
} ciostrm_features_t;



/**
 * This function tries to do a read for new data from the CamIO Stream called “this” and returns a read buffer pointer
 * called “buffer_o”. The read call may block. If the stream is empty, (e.g. end of file) or closed (e.g. disconnected) it
 * is valid to return 0 bytes. If padding is non-zero, the read will try to place data offset bytes into the buffer. This
 * may fail. You can check for success by comparing buffer_o->data_start and buffer_o->buffer_start pointers.
 * Return values:
 * - ENOERROR:  Completed successfully, buffer_o contains a valid structure.
 * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
 * - ENOSLOTS:  The stream could not allocate more buffers for the read. Free some buffers by releasing a read or write
 *              transaction.
 */
cioerr_t read_acquire( ciostrm_t* this, const ciobuff_rd_t* buffers_o, ch_word offset) ;


/**
 * Try to acquire a buffer for writing data into.  You can hang on to this slot as long as you like, but beware, most
 * streams offer a limited number of buffers, some streams can only acquire and release buffers sequentially and some
 * streams offer only one. If you are using stream association for zero-copy data movement, calling read_aquire has the
 * effect as calling write_acquire and/or visa-versa.
 * Returns:
 *  - ENOERROR: Completed successfully, buffer_o contains a valid structure.
 *  - ENOSLOTS: The stream could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 */
cioerr_t write_aquire(ciostrm_t* this, ciobuff_wr_t* buffers_o, ch_word* count_io);


/* Try to write data described by buffers to the given stream called “this”. Unlike POSIX, write_comit() returns the number
 * of bytes remaining rather than the number of bytes sent to make it easy to wait until a write is committed.
 * Returns:
 * - ENOERROR: Completed successfully.
 * - EQFULL: The queue is full. Cannot enqueue more slots. Call write with enqueue set to 0.
 * - ECOPYOP: A copy operation was required to complete this commit.
 * - or: the bytes remaining for this stream. Since writing is non-blocking, this may not always be 0.
 */
cioerr_t write_commit(ciostrm_t* this, ciobuff_wr_t* buffers, ch_word* bytes_remain);

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
cioerr_t read_release(ciostrm_t* this, ciobuff_rd_t* buffers);
cioerr_t write_release(ciostrm_t* this, ciobuff_wr_t* buffers);

/**
 * Free resources associated with this stream, but not with its connector.
 */
void destroy(ciostrm_t* this);


    /**
     * Return the metadata structure describing the properties of this transport.
     */
    ciostrm_inf info;


    /**
     * Return the the selectable structure for adding into a selector
     */
    cioselable selectable;

    /**
     * Pointer to stream specific data structures.
     */
    void* __priv;

};

struct cioconn_s {
    /**
     * Return the the selectable structure for adding into a selector
     */
    cioselable selectable;


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
    int (*connect)( cioconn* this, ciostrm* ciostrm_o );

    /**
     * Free resources associated with this connector, but not with any of the streams it has created.
     */
    void (*destroy)(cioconn* this);


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
cioerr_t new_cioconn( char* uri , ciostrm_req* properties, struct cioconn_s** cioconn_o );


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
cioerr_t new_ciostrm( char* uri ,  ciostrm_req* properties, ciosel selector, uint64_t timeout_ns, ciostrm** ciostream_o );


#endif /* STREAM_H_ */
