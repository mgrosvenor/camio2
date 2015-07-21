/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   01 Jul 2015
 *  File name: mfio_stream.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef SRC_DRIVERS_MFIO_MFIO_STREAM_H_
#define SRC_DRIVERS_MFIO_MFIO_STREAM_H_

#include <src/transports/stream.h>
#include <src/transports/connector.h>

NEW_STREAM_DECLARE(mfio);

camio_error_t mfio_stream_construct(
    camio_stream_t* this,
    camio_controller_t* connector,
    int base_fd,
    void* base_mem_start,
    ch_word base_mem_len
);

#endif /* SRC_DRIVERS_MFIO_MFIO_STREAM_H_ */
