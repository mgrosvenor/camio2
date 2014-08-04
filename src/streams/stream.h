/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 214
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
    camio_error_t (*read_acquire)( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o, ch_word buffer_offset,
                                ch_word source_offset);
    camio_error_t (*read_release)(camio_stream_t* this, camio_rd_buffer_t* buffer_chain);

    //Write operations
    camio_error_t (*write_aquire)(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io);
    camio_error_t (*write_commit)(camio_stream_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
                               ch_word dest_offset);
    camio_error_t (*write_release)(camio_stream_t* this, camio_wr_buffer_t* buffers_chain);

    void (*destroy)(camio_stream_t* this);

} camio_stream_interface_t;

/**
 * This is a basic CamIO stream structure. All streams must implement this. Streams will use the macros provided to overload
 * this structure with and include a private data structure.
 */
typedef struct camio_stream_s {

    /**
     * vtable that holds the function pointers, usually this would be a pointer to a constant, but saving 6x8bytes seems a
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
 * This function tries to do a read for new data from the CamIO Stream called “this” and returns a read buffer chain pointer
 * called “buffer_chain_o”. The read call may or may not block. Use a selector to ensure that the stream is ready for
 * reading. If the stream is empty, (e.g. end of file) or closed (e.g. disconnected) it is valid to return 0 bytes with an
 * ENOERROR. If buffer_offset is non-zero, the read will try to place data at offset bytes into each buffer. This may fail and
 * is only supported if the read_to_buff_off feature is supported. You can check for success by comparing
 * buffer_o->data_start and buffer_o->buffer_start pointers. If the source offset is non-zero, the read will try to retrieve
 * data from offset bytes from the beginning of the source. This may fail and is only supported if the read_from_src_off
 * feature is supported by the stream.
 * Return values:
 * - ENOERROR:  Completed successfully, buffer_o contains a valid structure.
 * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
 * - ENOBUFFS:  The stream could not allocate more buffers for the read. Free some buffers by releasing an outstanding read
 *              or write transaction.
 */
camio_error_t cmaio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o, ch_word buffer_offset,
                                  ch_word source_offset);

/**
 * Relinquish resources associated with the given read buffer chain. Streams with the async_arrv feature enabled support
 * asynchronous update modes. Read_release will check to see if the data in these buffer is still valid. If it is not valid,
 * it may have been trashed while you were working on it, so any results are invalid! If you are concerned about correctness,
 * you should a) not use a stream that has async_arrv b) ensure that your software can “roll back” when a corruption is
 * detected c) copy data and check for validity before proceeding. Some streams have sequencing requirements for buffer
 * chains. If you try to release an invalid sequence, an EBADSEQ error may be raised.
 * Return values:
 * - ENOERROR: All good, please continue
 * - EINVALID: Your data got trashed, time to recover!
 * - EBADSEQ:  You cannot release this buffer without releasing previous buffers in the sequence too
 */
camio_error_t camio_read_release(camio_stream_t* this, camio_rd_buffer_t* buffer_chain);



/**
 * Try to acquire count_io buffers for writing data into. You can hang on to these buffers as long as you like, but beware,
 * most streams offer a limited number of buffers, some streams can only acquire and release buffers sequentially and some
 * streams offer only one buffer. If you are using stream association for zero-copy data movement, calling read_aquire has
 * the effect of calling write_acquire and/or visa-versa. The number of buffers actually reserved will be returned in
 * count_io. This may be less than the original request. Some streams have sequencing requirements applied to buffer_chains
 * Returns:
 *  - ENOERROR: Completed successfully, buffer_o contains a valid structure.
 *  - ENOSLOTS: The stream could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 */
camio_error_t camio_write_aquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io);



/* Write data described by buffer_chain to the  stream called “this”. Write may or may not block. Use the selector loop
 * to ensure that the stream is ready for writing. If the dest_offset is non-zero, the write will try to place data starting
 * at offset bytes from the beginning of the destination. This may fail and is only supported if the write_to_dst_off
 * feature is supported by the stream. If buffer_offset is non-zero, the write will try to take data from offset bytes into
 * each buffer. This may fail and is only supported if the write_from_buff feature is supported by the stream.
 * Returns:
 * - ENOERROR: Completed successfully.
 * - TODO XXX: Add more here
 */
camio_error_t camio_write_commit(camio_stream_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
                           ch_word dest_offset);


/**
 * Relinquish resources associated with the given write buffer chain. If you try to release an invalid sequence, an EBADSEQ
 * error may be raised.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - EBADSEQ:  You cannot release this buffer without releasing previous buffers in the sequence too.
 */
camio_error_t camio_write_release(camio_stream_t* this, camio_wr_buffer_t* buffers_chain);


/**
 * Free resources associated with this stream, but not with its connector.
 */
void camio_destroy(camio_stream_t* this);



#endif /* STREAM_H_ */
