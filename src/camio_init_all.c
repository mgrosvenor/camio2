/*
 * camio_init_all.c
 *
 *  Created on: Oct 12, 2014
 *      Author: mgrosvenor
 */

//#include <src/drivers/netmap/netmap_device.h>
//#include <src/drivers/udp/udp_device.h>
//#include <src/drivers/tcp/tcp_device.h>
#include <src/drivers/delimiter/delim_device.h>
//#include <src/drivers/fileio/fio_device.h>
//#include <src/drivers/memfileio/mfio_device.h>
//#include <src/drivers/stdio/stdio_device.h>
#include <src/drivers/bring/bring_device.h>
#include <deps/chaste/utils/debug.h>

#include "camio_init_all.h"

// This is a dummy file to be filled in later with a compiler generated one. The aim is to have a configuration file that
// allows users to specific compiletime and runtime loading of modules.
// The reason for the strange callback like interface is to make it possible to for dynamic initialization of new devices.
// Even if a device doesn't exist in the CamIO library, it can be added at runtime.
// It also allows device to be loaded as dynamically linked objects, keeping the CamIO runtime minimal. CamIO is very much
// trying to emulate an interface similar to the linux kernel, where modules are dynamically loadable, or compiled in.



/**
 * TODO XXX: THIS FUNCTION SHOUD TO BE AUTO GENERATED!
 */
camio_error_t camio_init_all_devices(){


    DBG("Initializing devices...\n");
    //netmap_init();
//    udp_init();
//    tcp_init();
    delim_init();
//    fio_init();
//    mfio_init();
//    stdio_init();
    bring_init();
    //add more here

    DBG("Initializing devices...Done\n");
    return CAMIO_ENOERROR;
}


