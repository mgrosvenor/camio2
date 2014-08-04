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

#include "../types/types.h"

/**
 * Request (req) for stream features. Unless otherwise specified:
 *  1 = require stream to support feature,
 *  0 = require stream not to support feature,
 * -1 = don't care if stream supports feature
 *
 * Result (res) from stream feature request. Unless otherwise specified:
 *  1 = stream supports feature
 *  0 = stream does not support feature
 */
//
typedef struct camio_stream_features_s {
    ch_word readable;       // req/res: Does this stream support read operations (read_aquire, read_release)
    ch_word writeable;      // req/res: Does this stream support write operations (write_aquire, write_commit, write_release)
    ch_word reliable;       // req/res: Does this stream "guarantee" delivery. eg TCP.
    ch_word encrypted;      // req/res: Is it encrypted. eg SSL
    ch_word async_arrv;     // req/res: Can messages arrive asynchronously
    ch_word bytestream;     // req/res: Do messages arrive as a continuous stream of bytes, or as a datagram.
    ch_word can_read_off;   // req/res: Can the stream offset reads into the receive buffer at zero cost.
    ch_word mtu;            // req: -1 = Don't care
                            // req: >0 = Minimum value in bytes for maximum transfer unit for this stream
                            // res: Maximum transfer unit for this stream.
    ch_word scope;          // req:     -1 = Don't care,
                            // req/res   1 = at least local this machine only.
                            // req/res   2 = at least L2 point to point connectivity only,
                            // req/res   3 = L3 Global connectivity (eg IP).
    ch_word thread_safe;    // req/res: Calls to functions from multiple threads will not cause threading problems.
} camio_stream_features_t;




#endif /* FEATURES_H_ */
