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


#include "../types/types.h"
#include "../streams/transport_types.h"
#include "../streams/features.h"
#include "../buffers/buffer.h"
#include "../streams/connector.h"
#include "../streams/stream.h"
#include "api.h"

inline camio_error_t camio_connect( camio_connector_t* this, camio_stream_t** stream_o )
{
    return this->vtable.connect(this, stream_o);
}


inline void camio_connector_destroy(camio_connector_t* this)
{
    this->vtable.destroy(this);
}


inline camio_error_t camio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o, ch_word buffer_offset,
        ch_word source_offset)
{
    return this->vtable.read_acquire(this, buffer_chain_o, buffer_offset, source_offset);
}



inline camio_error_t camio_read_release(camio_stream_t* this, camio_rd_buffer_t* buffer_chain)
{
    return this->vtable.read_release(this, buffer_chain);
}



camio_error_t camio_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    return this->vtable.write_acquire(this, buffer_chain_o, count_io);
}



camio_error_t camio_write_commit(camio_stream_t* this, camio_wr_buffer_t* buffer_chain, ch_word buffer_offset,
        ch_word dest_offset)
{
    return this->vtable.write_commit(this, buffer_chain, buffer_offset, dest_offset);
}



camio_error_t camio_write_release(camio_stream_t* this, camio_wr_buffer_t* buffers_chain)
{
    return this->vtable.write_release(this, buffers_chain);
}



void camio_stream_destroy(camio_stream_t* this)
{
    this->vtable.destroy(this);
}



camio_error_t new_camio_transport( camio_connector_t** connector_o, camio_transport_type_t type,
        camio_transport_features_t* features,  void* parameters, ch_word parameters_size)
{
    (void)connector_o;
    (void)type;
    (void)features;
    (void)parameters;
    (void)parameters_size;

    return CAMIO_NOTIMPLEMENTED;
}


