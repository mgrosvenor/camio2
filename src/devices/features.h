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
 * -1 = it depends on some other condition
 */
//
typedef struct camio_device_features_s {
    ch_word readable;               // req/res: Do channels on this device support read messages
    ch_word read_to_buff_off;       // req/res: Can channels deposit read data at any offset in the receive buffer.
    ch_word read_from_src_off;      // req/res: Can channels read from an offset in the source channel (eg disk or RDMA)

    ch_word writeable;              // req/res: Do channels on this device support write operations
    ch_word write_from_buff_off;    // req/res: Can channels write from an offset in the write buffer.
    ch_word write_to_dst_off;       // req/res: Can channels write to an offset in the source buffer (eg disk or RDMA)

    ch_word reliable;               // req/res: Do channels "guarantee" delivery. eg TCP.
    ch_word encrypted;              // req/res: Are they encrypted. eg SSL
    ch_word inconsistent_rd;        // req/res: Is it possible that read operations might be inconsistent. If so, users must
                                    //          check the results of read_bufer_release to be sure that consistent data has
                                    //          been read.

    ch_word bytestream;             // req/res: Do messages arrive as a continuous stream of bytes, or as a datagram.
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
typedef struct camio_channel_features_s {
    camio_device_features_t features;
    ch_word rd_buff_req_max;
    ch_word rd_buff_req_slots;
    ch_word rd_data_req_max;
    ch_word rd_data_req_slots;

    ch_word wr_buff_req_max;
    ch_word wr_buff_req_slots;
    ch_word wr_data_req_max;
    ch_word wr_data_req_slots;
} camio_channel_features_t;




#endif /* FEATURES_H_ */
