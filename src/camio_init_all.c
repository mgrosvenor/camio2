/*
 * camio_init_all.c
 *
 *  Created on: Oct 12, 2014
 *      Author: mgrosvenor
 */

#include <src/drivers/netmap/netmap_transport.h>
#include "camio_init_all.h"


// This is a dummy file to be filled in later with a compiler generated one. The aim is to have a configuration file that
// allows users to specific compiletime and runtime loading of modules.
// The reason for the strange callback like interface is to make it possible to for dynamic initialization of new transports.
// Even if a transport doesn't exist in the CamIO library, it can be added at runtime.
// It also allows transport to be loaded as dynamically linked objects, keeping the CamIO runtime minimal.

camio_error_t camio_init_all_transports(){



    netmap_init();


    return CAMIO_ENOERROR;
}


