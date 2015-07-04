/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_connector.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <sys/stat.h>

#include <src/transports/connector.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include "bring_transport.h"
#include "bring_connector.h"
#include "bring_stream.h"


/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct bring_priv_s {

    bring_params_t* params;  //Parameters used when a connection happens

    //Read side
    bool is_rd_connected;  //Has connect be called?
    int rd_fd;             //File descriptor for the backing buffer
    void* rd_mem_start;    //Size and location of the mmaped region
    ch_word rd_mem_len;    //

    //Write side
    bool is_wr_connected;  //Has connect be called?
    int wr_fd;             //File descriptor for the backing buffer
    void* wr_mem_start;    //Size and location of the mmaped region
    ch_word wr_mem_len;    //



} bring_connector_priv_t;



/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

//Try to see if connecting is possible. With UDP, it is always possible.
static camio_error_t bring_connect_peek(camio_connector_t* this)
{
    DBG("Doing connect peek\n");
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->rd_fd > -1  && priv->wr_fd > -1){
        return CAMIO_ENOERROR; //Ready to go, please call connect!
    }

    volatile uint8_t* rd_bring = NULL;


    if(unlikely(camio_descr_has_opts(descr->opt_head))){
        eprintf_exit( "Option(s) supplied, but none expected\n");
    }


    //printf("Making bring ostream %s with %lu slots of size %lu\n", descr->query, priv->slot_count, priv->slot_size);


    //Make a local copy of the filename in case the descr pointer goes away (probable)
    size_t filename_len = strlen(descr->query);
    priv->filename = malloc(filename_len + 1);
    memcpy(priv->filename,descr->query, filename_len);
    priv->filename[filename_len] = '\0'; //Make sure it's null terminated


    //See if a bring file already exists, if so, get rid of it.
    bring_fd = open(descr->query, O_RDONLY);
    if(unlikely(bring_fd > 0)){
        wprintf("Found stale bring file. Trying to remove it.\n");
        close(bring_fd);
        if( unlink(descr->query) < 0){
            eprintf_exit("Could remove stale bring file \"%s\". Error=%s\n", descr->query, strerror(errno));
        }
    }


    bring_fd = open(descr->query, O_RDWR | O_CREAT | O_TRUNC , (mode_t)(0666));
    if(unlikely(bring_fd < 0)){
        eprintf_exit("Could not open file \"%s\". Error=%s\n", descr->query, strerror(errno));
    }

    //Resize the file
    if(lseek(bring_fd, CAMIO_BRING_MEM_SIZE -1, SEEK_SET) < 0){
        eprintf_exit( "Could not resize file for shared region \"%s\". Error=%s\n", descr->query, strerror(errno));
    }

    if(write(bring_fd, "", 1) < 0){
        eprintf_exit( "Could not resize file for shared region \"%s\". Error=%s\n", descr->query, strerror(errno));
    }

    bring = mmap( NULL, CAMIO_BRING_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, bring_fd, 0);
    if(unlikely(bring == MAP_FAILED)){
        eprintf_exit("Could not memory map bring file \"%s\". Error=%s\n", descr->query, strerror(errno));
    }

    //Initialize the bring with 0
    memset((uint8_t*)bring, 0, CAMIO_BRING_MEM_SIZE);


    priv->bring_size = CAMIO_BRING_MEM_SIZE;
    this->fd = bring_fd;
    priv->bring = bring;
    priv->curr = bring;
    bring_ostream_created = 1; //Tell a waiting reader that everything is initilaised
    priv->is_closed = 0;

    DBG("Mapped file to address %p with size %lli\n", priv->base_mem_start, priv->base_mem_len);

    //printf("%.*s\n",(int)priv->base_mem_len, priv->base_mem_start );

    return CAMIO_ENOERROR;

}

static camio_error_t bring_connector_ready(camio_muxable_t* this)
{
    if(this->fd > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = bring_connect_peek(this->parent.connector);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    return CAMIO_EREADY;
}

static camio_error_t bring_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = bring_connect_peek(this);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    if(priv->is_connected){
        DBG("Already connected! Why are you calling this twice?\n");
        return CAMIO_EALLREADYCONNECTED; // We're already connected!
    }

    //DBG("Done connecting, now constructing UDP stream...\n");
    camio_stream_t* stream = NEW_STREAM(bring);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = bring_stream_construct(stream, this, priv->base_fd, priv->base_mem_start, priv->base_mem_len);
    if(err){
       return err;
    }

    priv->is_connected = true;
    return CAMIO_ENOERROR;
}



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t bring_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(bring_params_t)){
        ERR("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    bring_params_t* bring_params = (bring_params_t*)(*params);
    priv->params = bring_params;

    //We require a hierarchical part!
    if(bring_params->hierarchical.str_len == 0){
        ERR("Expecting a hierarchical part in the BRING URI, but none was given. e.g my_bring1\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    //But only need a unique name. Not a full file path
    if(memchr(bring_params->hierarchical.str, "/", bring_params->hierarchical.str_len)){
        ERR("Found a / in the bring name.\n");
        return CAMIO_EINVALID;
    }
    if(memchr(bring_params->hierarchical.str, "\\", bring_params->hierarchical.str_len)){
        ERR("Found a / in the bring name.\n");
        return CAMIO_EINVALID;
    }

    priv->rd_fd = -1;
    priv->wr_fd = -1;

    //Populate the rest of the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = bring_connector_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void bring_destroy(camio_connector_t* this)
{
    DBG("Destorying bring connector\n");
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed connector structure\n");
}

NEW_CONNECTOR_DEFINE(bring, bring_connector_priv_t)
