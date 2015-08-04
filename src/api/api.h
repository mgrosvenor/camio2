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
#include <src/devices/features.h>
#include <src/multiplexers/mux.h>
#include <src/devices/channel.h>
#include <src/devices/device.h>


/**************************************************************************************************************************
 * Control Plane Operations - Controller
 **************************************************************************************************************************/
/**
 *  CamIO Control, Channel Request
 *  Tell the device that we would like to obtain a communication channel on this device. Some devices will support many
 *  channels. Some devices will support only one. The request is issued as a vector of request structures, with length
 *  vec_len_io. The device may have a limited number of request slots. The number of requests accepted into the queue is
 *  returned in vec_len_io.
 *  Return values:
 *  CAMIO_ENOERROR - the request was accepted into the queue
 *  CAMIO_ETOOMANY - the request slots have run-out
 *  CAMIO_ENOMORE  - the device has run out of communication channels, no more requests will succeed.
 */
camio_error_t camio_ctrl_chan_req( camio_dev_t* this, camio_msg_t* req_vec, ch_word* vec_len_io);


/**
 * CamIO Control, Channel Request Ready
 * The ready function checks to see if there is a new response (to a channel request) waiting. It will return
 * CAMIO_ENOERROR if there is a response waiting, or CAMIO_ETRYAGAIN if there is no response waiting.
 * Return values:
 * CAMIO_NOERROR - there is a new channel request response waiting
 *
 */
camio_error_t camio_ctrl_chan_ready( camio_dev_t* this );


/**
 *  CamIO Control, Channel Response / Result
 * When a channel status available, the  will return CAMIO_ENOERROR and the req_vec_o pointer will be populated with a
 * pointer to a valid channel. Otherwise, the result in req_vec_o is unspecified.
 *
 * Return values:
 * CAMIO_ENOERROR
 */
camio_error_t camio_ctrl_chan_res( camio_dev_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);


/**
 * Configure properties of the device
 */
//camio_error_t camio_ctrl_config( camio_dev_t* this, void* params, size_t param_size);


/**
 * Free resources associated with this device, but not with any of the channels it has created.
 */
void camio_device_destroy(camio_dev_t* this);


/**************************************************************************************************************************
 * Control Plane Operations - Multiplexer (Mux)
 **************************************************************************************************************************/


/**
 * Insert an object into a selector.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - ENOREAD: This selectable does not support reading
 * - ENOWRITE: This selectable does not support writing
 * - ENOCONNECT: This selectable does not support connecting
 * - TODO XXX: More errors here
 */
camio_error_t camio_mux_insert(camio_mux_t* this, camio_muxable_t* muxable, mux_callback_f callback, void* usr_state, ch_word id);


/**
 * Remove an object from a selector. Include a mode.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - TODO XXX: More errors here
 */
camio_error_t camio_mux_remove(camio_mux_t* this, camio_muxable_t* muxable);


/**
 * Perform the select operation. Find if any of the supplied channels are ready for reading, writing or connecting. The first
 * channel that matches one of these will fire. A camio_selector is trigger based. It has no memory. Once an event has been
 * delivered once, it will not be delivered again until there is a change in the state of the channel or device.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - TODO XXX: More errors here
 */
camio_error_t camio_mux_select(camio_mux_t* this, struct timeval* timeout, camio_muxable_t** muxable_o);


ch_word camio_mux_count(camio_mux_t* this);


/**
 * Free resources associated with this selector
 */
void camio_mux_destroy(camio_mux_t* this);



/**************************************************************************************************************************
 * Data Plane Operations - Read
 **************************************************************************************************************************/

/**
 * Try to acquire a buffer for read data into.
 */
camio_error_t camio_chan_rd_buff_req(camio_channel_t* this, camio_msg_t* req_vec,  ch_word* vec_len_io );

/**
 * Try to acquire a buffer for read data into.
 */
camio_error_t camio_chan_rd_buff_ready(camio_channel_t* this);

/**
 * Try get the results of a read buffer request
 */
camio_error_t camio_chan_rd_buff_res(camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);

/**
 * Relinquish resources associated with the given read buffer. Streams with the async_arrv feature enabled support
 * asynchronous update modes. Read_release will check to see if the data in these buffer is still valid. If it is not valid,
 * it may have been trashed while you were working on it, so any results are invalid! If you are concerned about correctness,
 * you should a) not use a channel that has async_arrv b) ensure that your software can âroll backâ when a corruption is
 * detected c) copy data and check for validity before proceeding. Some channels have sequencing requirements for buffer
 * chains. If you try to release an invalid sequence, an EBADSEQ error may be raised.
 * Return values:
 * - ENOERROR: All good, please continue
 * - EINVALID: Your data got trashed, time to recover!
 * - EBADSEQ:  You cannot release this buffer without releasing previous buffers in the sequence too
 */
camio_error_t camio_chan_rd_buff_release(camio_channel_t* this, camio_buffer_t* buffer);

/**
 * This function requests new data from the CamIO channel called 'this'. Read requests are supplied as a vector of
 * read_req_t structures. Each structure contains a destination buffer offset hint, a source offset hint and a read size
 * hint. The length of the vector is given by read_vec_len.
 *
 * When data is available, the ready() function will return EREADY and data will be returned by a read_acquire call. You
 * should not normally check ready() manually, but use a camio_multiplexer instead to do this. The maximum size of the
 * read_vec_len is channel dependent you can check the max_read_req_vec_len feature for this size. read_register() may be
 * called multiple times, but some channels will only support a limited number (e.g 1) of outstanding requests. You can check
 * the max_read_reqs feature for this size.
 *
 * Notes:
 * - If the source offset is non-zero, the read will try to retrieve data at offset bytes from the beginning of the source.
 *   This may fail and is only supported if the read_from_src_off feature is supported by the channel.
 * - If the destination offset is non-zero, the read will try to place data at offset bytes into the result buffer. This may
 *   fail and is only supported if the read_to_buff_off feature is supported.
 * - If a size hint is given, it gives an indication as to how much data the reader would like. Not all channels will respect
 *   this hint and many will give less than the desired hint.
 * - If the infinite_read feature is supported, you can supply a value of read_req_vec_len < 0. In this case, it is assumed
 *   that the read_req_vec contains precisely 1 entry. You will then only need to call read_req once and this entry will be
 *   repeated until the channel runs out of data.
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
camio_error_t camio_chan_rd_data_req( camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io );


/**
 * Check if the channel is ready to read. A device is ready if calling read will be non-blocking.
 * Return values:
 * - ENOERROR - the channel is ready to be read
 * - ETRYAGAIN - the channel is not ready to be read, try again later
 * - other - some other error has happened on the underlying channel
 */
camio_error_t camio_chan_rd_data_ready( camio_channel_t* this);


/**
 * This function returns a read buffer pointer called buffer_o. The read call may or may not block. You should use a
 * multiplexer to ensure that the channel is ready for reading. If the channel is empty, (e.g. end of file) or closed
 * (e.g. disconnected) it is valid to return an empty buffer with ENOERROR. If buffer_offset is non-zero.
 * Return values:
 * - ENOERROR:  Completed successfully, buffer_o contains a valid structure.
 * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
 * - ENOBUFFS:  The channel could not allocate more buffers for the read. Free some buffers by releasing an outstanding read
 *              or write transaction.
 */
camio_error_t camio_chan_rd_data_res( camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);





/**************************************************************************************************************************
 * Data Plane Operations - Write
 **************************************************************************************************************************/
/**
 * Try to acquire a buffer for writing data into. You can hang on to this buffers as long as the channel permits, but beware,
 * most channels offer a limited number of buffers, some channels can only acquire and release buffers sequentially and some
 * channels offer only one buffer. If you are using channel association for zero-copy data movement, calling read_aquire has
 * the effect of calling write_acquire.
 * Returns:
 *  - ENOERROR: Completed successfully, buffer_o contains a valid structure.
 *  - ENOSLOTS: The channel could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 */
camio_error_t camio_chan_wr_buff_req(camio_channel_t* this, camio_msg_t* req_vec,  ch_word* vec_len_io );

/**
 * Try to acquire a buffer for writing data into. You can hang on to this buffers as long as the channel permits, but beware,
 * most channels offer a limited number of buffers, some channels can only acquire and release buffers sequentially and some
 * channels offer only one buffer. If you are using channel association for zero-copy data movement, calling read_aquire has
 * the effect of calling write_acquire.
 * Returns:
 *  - ENOERROR: Completed successfully, buffer_o contains a valid structure.
 *  - ENOSLOTS: The channel could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 */
camio_error_t camio_chan_wr_buff_ready(camio_channel_t* this);

/**
 * Try get the results of a write buffer request
 */
camio_error_t camio_chan_wr_buff_res(camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);


/* Write data described by the , or buffer_chain to the channel called 'this'. Write may or may not block. Use a
 * selector loop to ensure that the channel is ready for writing if you do not wish to block. In some channels, the act of
 * calling write will cause the write buffer to become automatically released. The writer will return EAUTORELEASE in this
 * case. This means that you cannot re-use the write buffer, and you must call read_aquire again to get another buffer.
 * Returns:
 * - ENOERROR: Completed successfully.
 * - TODO XXX: Add more here
 */
camio_error_t camio_chan_wr_data_req(camio_channel_t* this, camio_msg_t* req_vec,  ch_word* vec_len_io );

/**
 * Check if the channel is ready to write.
 */
camio_error_t camio_chan_wr_data_ready( camio_channel_t* this);

/**
 * Try get the results of a write request
 */
camio_error_t camio_chan_wr_data_res(camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io);



/**
 * Relinquish resources associated with the given write buffer If you try to release an invalid sequence, an EBADSEQ
 * error may be raised.
 * Return values:
 * - ENOERROR: All good, please continue.
 * - EBADSEQ:  You cannot release this buffer without releasing previous buffers in the sequence too.
 */
camio_error_t camio_chan_wr_buff_release(camio_channel_t* this, camio_buffer_t* buffer);


/**
 * Free resources associated with this channel, but not with its device.
 */
void camio_channel_destroy(camio_channel_t* this);


/**************************************************************************************************************************
 * Data Plane Operations - Read <--> Write
 **************************************************************************************************************************/

/**
 * Copy contents of read buffer into write buffer. This may or may not involve an actual data copy depending on the channel
 * bindings currently in use. The function may succeed in in one of two ways:
 *  - EBYTESCOPPIED:    Completed successfully, bytes were copied
 *  - ENOBYTESCOPPIED:  Completed successfully, no bytes were copied.
 */
camio_error_t camio_copy_rw(
        camio_buffer_t** wr_buffer,
        camio_buffer_t** rd_buffer,
        ch_word src_offset,
        ch_word dst_offset,
        ch_word copy_len
        );

/**
 * Ignore the contents of a a read buffer. Some channels can provide performance improvements from the knowledge that data is
 * being ignored. This may reduce the need for unnecessary copies. It is good practice to use this whenever read data is not
 * going to be copied.
 *
 * Returns:
 *  - ENOERROR: The operation completed successfully
 *  - TODO XXX: Fill in more here
 */
camio_error_t camio_ignore(camio_buffer_t** wr_buffer, camio_buffer_t** rd_buffer, ch_word offset, ch_word copy_len);



#endif /* API_H_ */
