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


/**
 * Every CamIO stream must implement this interface, see function prototypes later for full details
 */
typedef struct camio_stream_interface_s{
    //Read operations
    camio_error_t (*read_acquire)(camio_stream_t* this,  camio_rd_buffer_t* buffer_chain_o, ch_word offset_hint) ;
    camio_error_t (*read_release)(camio_stream_t* this, camio_rd_buffer_t* buffer_chain);

    //Write operations
    camio_error_t (*write_aquire)(camio_stream_t* this, camio_wr_buffer_t* buffer_chain_o, ch_word* count_io);
    camio_error_t (*write_commit)(camio_stream_t* this, camio_rd_buffer_t* buffer_chain, ch_word* bytes_remain_o);
    camio_error_t (*write_release)(camio_stream_t* this, camio_wr_buffer_t* buffer_chain);

    void (*destroy)(camio_stream_t* this);

} camio_stream_interface_t;

/**
 * This is a basic CamIO stream structure. All streams must implement this. Streams will use the macros provided to overload
 * this structure with and include a private data structure.
 */
typedef struct camio_stream_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer to a constan, but saving 6x8bytes seems a
     * little silly when it will cost a pointer dereference on the critical
     * path.
     */
    camio_stream_interface_t vtable;

    /**
     * Holds the meta-data structure describing the properties of this transport.
     */
    camio_stream_features_t features;


    /**
     * Holds the selectable structure for adding into a selector
     */
    camio_selectable_t selectable;


} camio_stream_t;



/**
 * This function tries to do a read for new data from the CamIO Stream called “this” and returns a read buffer pointer
 * called “buffer_o”. The read call may or may not block. If the stream is empty, (e.g. end of file) or closed (e.g.
 * disconnected) it is valid to return 0 bytes. If offset_hint is non-zero, the read will try to place data offset bytes into
 * the buffer. This may fail. You can check for success by comparing buffer_o->data_start and buffer_o->buffer_start
 * pointers.
 * Return values:
 * - ENOERROR:  Completed successfully, buffer_o contains a valid structure.
 * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
 * - ENOBUFFS:  The stream could not allocate more buffers for the read. Free some buffers by releasing an outstanding read
 *              or write transaction.
 */
camio_error_t read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o, ch_word offset_hint) ;



/**
 * Try to acquire count_io buffers for writing data into. You can hang on to these buffers as long as you like, but beware,
 * most streams offer a limited number of buffers, some streams can only acquire and release buffers sequentially and some
 * streams offer only one. If you are using stream association for zero-copy data movement, calling read_aquire has the
 * effect as calling write_acquire and/or visa-versa. The number of buffers actually reserved will be returned in count_io.
 * This may be less than the original request.
 * Returns:
 *  - ENOERROR: Completed successfully, buffer_o contains a valid structure.
 *  - ENOSLOTS: The stream could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 */
camio_error_t write_aquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io);


/* Write data described by buffer_chain to the given stream called “this”. Write may or may not block. Use
 * Returns:
 * - ENOERROR: Completed successfully.
 * - ECOPYOP:  A copy operation was required to complete this commit.This may happen if the write buffer belonged to another
 *             stream.
 */
camio_error_t write_commit(camio_stream_t* this, camio_wr_buffer_t* buffer_chain);



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
camio_error_t read_release(camio_stream_t* this, camio_rd_buffer_t* buffer_chain);
camio_error_t write_release(camio_stream_t* this, camio_wr_buffer_t* buffers_chain);


/**
 * Free resources associated with this stream, but not with its connector.
 */
void destroy(camio_stream_t* this);



#endif /* STREAM_H_ */
