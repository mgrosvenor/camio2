/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: stdio_device.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <src/devices/device.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include <src/drivers/fileio/fio_device.h>
#include "stdio_device.h"
#include "stdio_device.h"


/**************************************************************************************************************************
 * Connect functions --proxy all of these functions and data through to fio. Doing this means there's no runtime cost
 **************************************************************************************************************************/
typedef fio_device_priv_t stdio_device_priv_t;

extern camio_error_t fio_devect_peek(camio_device_t* this);
#define stdio_devect_peek fio_devect_peek

extern camio_error_t fio_devect_ready(camio_device_t* this);
#define stdio_devect_ready fio_devect_ready

extern camio_error_t fio_devect(camio_device_t* this, camio_channel_t** channel_o );
#define stdio_devect fio_devect



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/
extern camio_error_t fio_construct(camio_device_t* this, void** params, ch_word params_size);
static camio_error_t stdio_construct(camio_device_t* this, void** params, ch_word params_size)
{

    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(stdio_params_t)){
        DBG("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    stdio_params_t* stdio_params = (stdio_params_t*)(*params);

    if(stdio_params->__hierarchical__.str_len != 0 ){
        ERR("stdio does not accept file names!\n");
        return CAMIO_EINVALID;
    }

    DBG("Constucting with params rd_sz=%lli wr_sz=%lli ro=%lli wo=%lli\n",
       stdio_params->rd_buff_sz,
       stdio_params->wr_buff_sz,
       stdio_params->rd_only,
       stdio_params->wr_only);

    fio_params_t* fio_params = (fio_params_t*)calloc(1,sizeof(fio_params_t));

    fio_params->rd_fd = STDIN_FILENO;
    fio_params->wr_fd = stdio_params->std_err ? STDERR_FILENO : STDOUT_FILENO;

    if(stdio_params->rd_only){
        fio_params->wr_fd = -1;
        fio_params->rd_only = 1;
    }

    if(stdio_params->wr_only){
        fio_params->rd_fd = -1;
        fio_params->wr_only = 1;
    }

    fio_params->rd_buff_sz = stdio_params->rd_buff_sz;
    fio_params->wr_buff_sz = stdio_params->wr_buff_sz;

    free(*params);
    *params = NULL;

    return fio_construct(this, (void**)&fio_params,sizeof(fio_params_t));
}


extern void fio_destroy(camio_device_t* this);
#define stdio_destroy fio_destroy

NEW_DEVICE_DEFINE(stdio, stdio_device_priv_t)
