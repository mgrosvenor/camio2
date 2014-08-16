/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 16, 2014
 *  File name: netmap_connector.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef NETMAP_CONNECTOR_H_
#define NETMAP_CONNECTOR_H_

#include <stdarg.h>

camio_error_t new_netmap_connector_bin_va( camio_connector_t** connector_o, camio_stream_features_t* properties,
        va_list args );

camio_error_t new_netmap_connector_uri( camio_connector_t** connector_o, char* uri , camio_stream_features_t* properties );

#endif /* NETMAP_CONNECTOR_H_ */
