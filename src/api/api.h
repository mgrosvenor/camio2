/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: api.h
 *  Description:
 *  This file includes the most basic API for accessing CamIO which include all possible CamIO functionality. As a result, it
 *  is the most difficult, but most powerful way to drive CamIO.  Many of the functions within the API will be used in regular
 *  patterns for simpler systems so it is expected that these will be wrapped up into wrapper APIs for more common use.
 */

#ifndef API_H_
#define API_H_

#include <src/types/types.h>
#include <src/errors/camio_errors.h>

#include <src/buffers/buffer.h>
#include <src/transports/features.h>
#include <src/multiplexers/mux.h>
#include <src/transports/stream.h>


/**
 *  Attempt to connect the underlying stream to it's data source. This may or may not block. If successful, the CamIO stream
 *  can be read and/or written to. In many cases, the connect operation will return immediately, with a valid stream once
 *  only. However, this is not guaranteed. Some streams may return multiple valid connection and some streams may take some
 *  time before they return successfully. Streams can be placed into selectors to facilitate the blocking behavior
 *  necessary to wait for these events to happen by listening for the  on connection signal.
 *  Return values;
 *  - ENOERROR: Stream was created successfully.
 *  - ETRYAGAIN: The stream has nothing new to report at this time. No connection has yet happened.
 *  - ECHECKERROR: The stream creation has failed.
 */
camio_error_t camio_connect( camio_connector_t* this, camio_stream_t** stream_o );


/**
 * Free resources associated with this connector, but not with any of the streams it has created.
 */
void camio_connector_destroy(camio_connector_t* this);

/**
 * Check if the connector is ready to connect. A connector is ready if calling connect will be non-blocking.
 */
camio_error_t camio_connector_ready( camio_connector_t* this);

/**
 * This function requests new data from the CamIO Stream called 'this'. Read requests are supplied as a vector of
 * read_req_t structures. Each structure contains a destination buffer offset hint, a source offset hint and a read size
 * hint. The length of the vector is given by read_vec_len.
 *
 * When data is available, the ready() function will return EREADY and data will be returned by a read_acquire call. You
 * should not normally check ready() manually, but use a camio_multiplexer instead to do this. The maximum size of the
 * read_vec_len is stream dependent you can check the max_read_req_vec_len feature for this size. read_register() may be
 * called multiple times, but some streams will only support a limited number (e.g 1) of outstanding requests. You can check
 * the max_read_reqs feature for this size.
 *
 * Notes:
 * - If the source offset is non-zero, the read will try to retrieve data at offset bytes from the beginning of the source.
 *   This may fail and is only supported if the read_from_src_off feature is supported by the stream.
 * - If the destination offset is non-zero, the read will try to place data at offset bytes into the result buffer. This may
 *   fail and is only supported if the read_to_buff_off feature is supported.
 * - If a size hint is given, it gives an indication as to how much data the reader would like. Not all streams will respect
 *   this hint and many will give less than the desired hint.
 * - If the infinite_read feature is supported, you can supply a value of read_req_vec_len < 0. In this case, it is assumed
 *   that the read_req_vec contains precisely 1 entry. You will then only need to call read_req once and this entry will be
 *   repeated until the stream runs out of data.
 *
 * Return values:
 * - ENOERROR:      Completed successfully, read request has been registered
 * - ETOOMANY:      Too many read requests registered. This one has been ignored
 * - EOVERSIZEVEC:  The vector is too big to be registered.
 * - TODO XXX: Fill in more here
 *
 * TODO XXX: Another way to build this interface would be to register the buffer pointer here as well. I feel that it is less
 * elegant as the meaning of the API is now obscured, but it could be faster. Hmmm.. Defer to a later decision point
 */
camio_error_t camio_read_request( camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len );


/**
 * Check if the stream is ready to read. A connector is ready if calling read will be non-blocking.
 * Return values:
 * - EREADY - the stream is ready to be read
 * - ENOTREDY - the stream is not ready to be read, try again later
 * - other - some other error has happened on the underlying stream
 */
camio_error_t camio_read_ready( camio_stream_t* this);


/**
 * This function returns a read buffer pointer called buffer_o. The read call may or may not block. You should use a
 * multiplexer to ensure that the stream is ready for reading. If the stream is empty, (e.g. end of file) or closed
 * (e.g. disconnected) it is valid to return an empty buffer with ENOERROR. If buffer_offset is non-zero.
 * Return values:
 * - ENOERROR:  Completed successfully, buffer_o contains a valid structure.
 * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
 * - ENOBUFFS:  The stream could not allocate more buffers for the read. Free some buffers by releasing an outstanding read
 *              or write transaction.
 */
camio_error_t camio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o);


/**
 * Relinquish resources associated with the given read buffer. Streams with the async_arrv feature enabled support
 * asynchronous update modes. Read_release will check to see if the data in these buffer is still valid. If it is not valid,
 * it may have been trashed while you were working on it, so any results are invalid! If you are concerned about correctness,
 * you should a) not use a stream that has async_arrv b) ensure that your software can âroll backâ when a corruption is
 * detected c) copy data and check for validity before proceeding. Some streams have sequencing requirements for buffer
 * chains. If you try to release an invalid sequence, an EBADSEQ error may be raised.
 * Return values:
 * - ENOERROR: All good, please continue
 * - EINVALID: Your data got trashed, time to recover!
 * - EBADSEQ:  You cannot release this buffer without releasing previous buffers in the sequence too
 */
camio_error_t camio_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer);


/**
 * Try to acquire a buffer for writing data into. You can hang on to this buffers as long as the stream permits, but beware,
 * most streams offer a limited number of buffers, some streams can only acquire and release buffers sequentially and some
 * streams offer only one buffer. If you are using stream association for zero-copy data movement, calling read_aquire has
 * the effect of calling write_acquire.
 * Returns:
 *  - ENOERROR: Completed successfully, buffer_o contains a valid structure.
 *  - ENOSLOTS: The stream could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 */
camio_error_t camio_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o);

/**
 * Check if the stream is ready to write. A connector is ready if calling write will be non-blocking.
 */
camio_error_t camio_write_ready( camio_stream_t* this);


/**
 * Ignore the contents of a a read buffer. Some streams can provide performance improvements from the knowledge that data is
 * being ignored. This may reduce the need for unnecessary copies. It is good practice to use this whenever read data is not
 * going to be copied.
 *
 * Returns:
 *  - ENOERROR: The operation completed successfully
 *  - TODO XXX: Fill in more here
 */
camio_error_t camio_ignore(camio_wr_buffer_t** wr_buffer, camio_rd_buffer_t** rd_buffer, ch_word offset, ch_word copy_len);


/**
 * Copy contents of read buffer into write buffer. This may or may not involve an actual data copy depending on the stream
 * bindings currently in use. The function may succeed in in one of two ways:
 *  - EBYTESCOPPIED:    Completed successfully, bytes were copied
 *  - ENOBYTESCOPPIED:  Completed successfully, no bytes were copied.
 */
camio_error_t camio_copy_rw(
        camio_wr_buffer_t** wr_buffer,
        camio_rd_buffer_t** rd_buffer,
        ch_word src_offset,
        ch_word dst_offset,
        ch_word copy_len
        );

/* Write data described by the , or buffer_chain to the stream called 'this'. Write may or may not block. Use a
 * selector loop to ensure that the stream is ready for writing if you do not wish to block. In some streams, the act of
 * calling write will cause the write buffer to become automatically released. The writer will return EAUTORELEASE in this
 * case. This means that you cannot re-use the write buffer, and you must call read_aquire again to get another buffer.
 * Returns:
 * - ENOERROR: Completed successfully.
 * - TODO XXX: Add more here
 */
camio_error_t camio_write_request(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len);


/**
 * Relinquish resources associated with the given write buffer If you try to release an invalid sequence, an EBADSEQ
 * error may be raised.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - EBADSEQ:  You cannot release this buffer without releasing previous buffers in the sequence too.
 */
camio_error_t camio_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer);


/**
 * Free resources associated with this stream, but not with its connector.
 */
void camio_stream_destroy(camio_stream_t* this);


/**
 * Insert an object into a selector. Include a mode. For selectors that do not support the mode of operation requested, an
 * error will be returned. Modes can be OR'd together for example ( CAMIO_SELECTOR_MODE_WRITE | AMIO_SELECTOR_MODE_READ).
 * Return values:
 * - ENOERROR: All good, please continue.
 * - ENOREAD: This selectable does not support reading
 * - ENOWRITE: This selectable does not support writing
 * - ENOCONNECT: This selectable does not support connecting
 * - TODO XXX: More errors here
 */
camio_error_t camio_mux_insert(camio_mux_t* this, camio_muxable_t* muxable, ch_word id);


/**
 * Remove an object from a selector. Include a mode.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - TODO XXX: More errors here
 */
camio_error_t camio_mux_remove(camio_mux_t* this, camio_muxable_t* muxable);


/**
 * Perform the select operation. Find if any of the supplied streams are ready for reading, writing or connecting. The first
 * stream that matches one of these will fire. A camio_selector is trigger based. It has no memory. Once an event has been
 * delivered once, it will not be delivered again until there is a change in the state of the stream or connector.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - TODO XXX: More errors here
 */
camio_error_t camio_mux_select(camio_mux_t* this, /*struct timespec timeout,*/ camio_muxable_t** muxable_o);


/**
 * Free resources associated with this selector
 */
void camio_mux_destroy(camio_mux_t* this);


#endif /* API_H_ */
