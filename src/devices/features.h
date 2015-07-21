/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 3, 2014
 *  File name: features.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef FEATURES_H_
#define FEATURES_H_

#include <src/types/types.h>

/**
 * Request (req) for device features. Unless otherwise specified:
 *  1 = require device to support feature,
 *  0 = require device not to support feature,
 * -1 = don't care if device supports feature
 *
 * Result (res) from device feature request. Unless otherwise specified:
 *  1 = device supports feature
 *  0 = device does not support feature
 */
//
typedef struct camio_device_features_s {
    ch_word readable;               // req/res: Does this channel support read operations (read_aquire & read_release)
    ch_word read_to_buff_off;       // req/res: Can the channel deposit read data at any offset in the receive buffer.
    ch_word read_from_src_off;      // req/res: Can the channel read from an offset in the source channel (eg disk or RDMA)

    ch_word writeable;              // req/res: Does this channel support write operations (write_aquire, write_commit,
                                    // & write_release)
    ch_word write_from_buff_off;    // req/res: Can the channel write from any offset in the write buffer.
    ch_word write_to_dst_off;       // req/res: Can the channel w from an offset in the source (eg disk or RDMA)

    ch_word reliable;               // req/res: Does this channel "guarantee" delivery. eg TCP.
    ch_word encrypted;              // req/res: Is it encrypted. eg SSL
    ch_word async_arrv;             // req/res: Can messages arrive asynchronously. Specifically, is it possible that read
                                    //          operations might be inconsistent. If so, users must check the results of
                                    //          read_release to be sure that consistent data has been read.

    ch_word bytechannel;             // req/res: Do messages arrive as a continuous channel of bytes, or as a datagram.
    ch_word mtu;                    // req:     -1 = Don't care
                                    // req:     >0 = Minimum value in bytes for maximum transfer unit for this channel
                                    // res:     Maximum transfer unit for this channel.

    ch_word scope;                  // req:     -1 = Don't care,
                                    // req/res   1 = at least local this machine only.
                                    // req/res   2 = at least L2 point to point connectivity only,
                                    // req/res   3 = L3 Global connectivity (eg IP).

    ch_word thread_safe;            // req/res: Calls to functions from multiple threads will not cause threading problems.
                                    //          this may introduce performance bottlenecks. Thread safety is both a compile-
                                    //          time and a runtime option. Both should be enabled for thread safety.
} camio_device_features_t;




#endif /* FEATURES_H_ */
