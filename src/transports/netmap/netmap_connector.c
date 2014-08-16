/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: netmap_connector.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "connector.h"

static camio_error_t connect( camio_connector_t* this, camio_stream_t* stream_o )
{
    return CAMIO_NOTIMPLEMENTED;
}

static void connector_destroy(camio_connector_t* this)
{

}

const static camio_connector_interface_t connector_interface = {
        .connect = connect,
        .destroy = connector_destroy,
};

static camio_connector_t connector = {
        .vtable = connector_interface
        /*.selectable = TODO XXX*/
};


camio_error_t connector_new_bin( camio_connector_t** connector_o, camio_stream_type_t type,
        camio_stream_features_t* properties,  ... )
{
    return CAMIO_NOTIMPLEMENTED;
}


camio_error_t connector_new_uri( camio_connector_t** connector_o, char* uri , camio_stream_features_t* properties )
{
    return CAMIO_NOTIMPLEMENTED;
}

