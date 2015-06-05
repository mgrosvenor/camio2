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
//#include <src/streams/stream_types.h>
//#include <src/streams/features.h>
#include <src/buffers/buffer.h>
#include <src/transports/features.h>


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
 * This function tries to do a read for new data from the CamIO Stream called 'this' and returns a read buffer pointer called
 * buffer_o. The read call may or may not block. You should use a selector to ensure that the stream is ready for reading. If
 * the stream is empty, (e.g. end of file) or closed (e.g. disconnected) it is valid to return an empty buffer with ENOERROR.
 * If buffer_offset is non-zero, the read will try to place data at offset bytes into each buffer. This may fail and
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
camio_error_t camio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o, ch_word buffer_offset,
        ch_word source_offset);

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
camio_error_t camio_copy_rw(camio_wr_buffer_t** wr_buffer, camio_rd_buffer_t** rd_buffer, ch_word offset, ch_word copy_len);


/**
 * Set options on a write buffer. If the dest_offset is non-zero, the write will try to collect data starting
 * at offset bytes from the beginning of the destination. This may fail and is only supported if the write_to_dst_off
 * feature is supported by the stream. If buffer_offset is non-zero, the write will try to write data at offset bytes into
 * the stream. This may fail and is only supported if the write_from_buff feature is supported by the stream.
 */
camio_error_t set_opts(camio_wr_buffer_t** wr_buffer,  ch_word buffer_offset, ch_word dest_offset);


/**
 * A single buffer object is a buffer chain of length 1. If you require batching performance, you can append additional write
 * buffers to form a longer chain. Some streams may have ordering requirements on appending chains and many streams will have
 * limits on the length of chains that are permitted.
 * * Returns:
 * - ENOERROR: Completed successfully.
 * - TODO XXX: Add more here
 */
camio_error_t camio_chain(camio_wr_buffer_t** buffer_chain_head, camio_wr_buffer_t** buffer );


/* Write data described by the buffer, or buffer_chain to the stream called 'this'. Write may or may not block. Use a
 * selector loop to ensure that the stream is ready for writing if you do not wish to block. In some streams, the act of
 * calling write will cause the write buffer to become automatically released. The writer will return EAUTORELEASE in this
 * case. This means that you cannot re-use the write buffer, and you must call read_aquire again to get another buffer.
 * Returns:
 * - ENOERROR: Completed successfully.
 * - TODO XXX: Add more here
 */
camio_error_t camio_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain );


/**
 * Relinquish resources associated with the given write buffer chain. If you try to release an invalid sequence, an EBADSEQ
 * error may be raised.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - EBADSEQ:  You cannot release this buffer without releasing previous buffers in the sequence too.
 */
camio_error_t camio_write_release(camio_stream_t* this, camio_wr_buffer_t** buffers_chain);


/**
 * Free resources associated with this stream, but not with its connector.
 */
void camio_stream_destroy(camio_stream_t* this);


//
///**
// * Create a new CamIO transport of type "type" with the properties requested and otherwise binary arguments, return its
// * connector. The type and order of these arguments is stream specific. The camio_connector_o is only valid id ENOERROR is
// * returned.
// * Returns:
// * - ENOERROR:  All good. Nothing to see here.
// * - ENOSTREAM: The stream type is unrecognized
// * - EBADOPT:   The arguments have an error or are unsupported
// * - EBADPROP:  The stream supplied in did not match the properties requested.
// */
//camio_error_t new_camio_transport( camio_connector_t** connector_o, camio_transport_type_t type,
//        camio_transport_features_t* features,  void* parameters, ch_word parameters_size);
//



#endif /* API_H_ */
