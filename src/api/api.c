/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 17, 2014
 *  File name: api.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


/*
 * CamIO - The Cambridge Input/Output API
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details.
 *
 *  Created:   Aug 15, 2014
 *  File name: api.c
 *  Description:
 *  <INSERT DESCRIPTION HERE>
 */


#include <src/transports/connector.h>
#include <src/transports/features.h>
#include <src/transports/stream.h>
#include <src/errors/camio_errors.h>
#include "../types/types.h"
#include "../buffers/buffer.h"
#include "../camio_debug.h"
#include "api.h"


inline camio_error_t camio_connect( camio_connector_t* this, camio_stream_t** stream_o )
{
    return this->vtable.connect(this, stream_o);
}


inline void camio_connector_destroy(camio_connector_t* this)
{
    this->vtable.destroy(this);
}


camio_error_t camio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o, ch_word buffer_offset,
        ch_word source_offset)
{
    return this->vtable.read_acquire(this, buffer_o, buffer_offset, source_offset);
}



camio_error_t camio_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    return this->vtable.read_release(this, buffer);
}



camio_error_t camio_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    return this->vtable.write_acquire(this, buffer_o);
}



camio_error_t camio_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain )
{
    return this->vtable.write_commit(this, buffer_chain);
}



camio_error_t camio_write_release(camio_stream_t* this, camio_wr_buffer_t** buffers_chain)
{
    return this->vtable.write_release(this, buffers_chain);
}



void camio_stream_destroy(camio_stream_t* this)
{
    this->vtable.destroy(this);
}



/**
 * Copy contents of read buffer into write buffer. This may or may not involve an actual data copy depending on the stream
 * bindings currently in use. The function may succeed in in one of two ways:
 *  - EBYTESCOPPIED:    Completed successfully, bytes were copied
 *  - ENOBYTESCOPPIED:  Completed successfully, no bytes were copied.
 */
camio_error_t camio_copy_rw(camio_wr_buffer_t** wr_buffer, camio_rd_buffer_t** rd_buffer, ch_word offset, ch_word copy_len)
{
    (void)offset;

    //TODO XXX this is temporary until the actual copy function is implemented
    //DBG("Copying %lli bytes from %p to %p\n",rd_buffer->data_len,rd_buffer->buffer_start, wr_buffer->buffer_start);
    memcpy( (*wr_buffer)->buffer_start, (*rd_buffer)->data_start, copy_len);
    (*wr_buffer)->data_start = (*wr_buffer)->buffer_start;
    (*wr_buffer)->data_len = (*rd_buffer)->data_len;
    //DBG("Done copying %lli bytes from %p to %p\n",rd_buffer->data_len,rd_buffer->buffer_start, wr_buffer->buffer_start);

    return CAMIO_ENOERROR;
}

/**
 * Set options on a write buffer. If the dest_offset is non-zero, the write will try to collect data starting
 * at offset bytes from the beginning of the destination. This may fail and is only supported if the write_to_dst_off
 * feature is supported by the stream. If buffer_offset is non-zero, the write will try to write data at offset bytes into
 * the stream. This may fail and is only supported if the write_from_buff feature is supported by the stream.
 */
camio_error_t camio_set_opts(camio_wr_buffer_t** wr_buffer,  ch_word buffer_offset, ch_word dest_offset)
{
    (void)wr_buffer;
    (void)buffer_offset;
    (void)dest_offset;
    return CAMIO_NOTIMPLEMENTED;
}

/**
 * A single buffer object is a buffer chain of length 1. If you require batching performance, you can append additional write
 * buffers to form a longer chain. Some streams may have ordering requirements on appending chains and many streams will have
 * limits on the length of chains that are permitted.
 * * Returns:
 * - ENOERROR: Completed successfully.
 * - TODO XXX: Add more here
 */
camio_error_t camio_chain(camio_wr_buffer_t** buffer_chain_head, camio_wr_buffer_t** buffer )
{
    (void)buffer_chain_head;
    (void)buffer;
    return CAMIO_NOTIMPLEMENTED;
}





