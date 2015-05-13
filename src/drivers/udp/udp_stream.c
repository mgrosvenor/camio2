/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: udp_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include <src/transports/stream.h>
#include <src/transports/connector.h>

#include "../../camio_debug.h"

#include "udp_stream.h"
#include "udp_buffer.h"

#include <fcntl.h>
#include <errno.h>

#define CAMIO_UDP_STREAM_BUFFERS 256
#define CAMIO_UDP_BUFFER_SIZE (64 * 1024) //Allow a full IP segment of data
typedef struct udp_stream_priv_s {
    camio_connector_t connector;
    char* rd_buffers[CAMIO_UDP_STREAM_BUFFERS];
    camio_buffer_t rd_buffer_hdrs[CAMIO_UDP_STREAM_BUFFERS];
    ch_word curr_rd_buff;

    char* wr_buffers[CAMIO_UDP_STREAM_BUFFERS];
    camio_buffer_t wr_buffer_hdrs[CAMIO_UDP_STREAM_BUFFERS];
    ch_word curr_wr_buff;

    int rd_fd;
    int wr_fd;

} udp_stream_priv_t;


static void init_buffer_hdr(camio_stream_t* this, camio_buffer_t* buffer_hdr, char* buffer_data, ch_word buffer_id,
        ch_bool read_only){
    buffer_hdr->buffer_len                     = CAMIO_UDP_BUFFER_SIZE;
    buffer_hdr->buffer_start                   = buffer_data;
    buffer_hdr->_internal.__buffer_id          = buffer_id;
    buffer_hdr->_internal.__buffer_parent      = this;
    buffer_hdr->_internal.__do_auto_release    = false;
    buffer_hdr->_internal.__in_use             = false;
    buffer_hdr->_internal.__pool_id            = 0;
    buffer_hdr->_internal.__read_only          = read_only;
}


static void udp_destroy(camio_stream_t* this)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->rd_fd > -1){
        close(priv->rd_fd);
        priv->rd_fd = -1; //Make this reentrant safe
    }

    if(priv->wr_fd > -1){
        close(priv->wr_fd);
        priv->wr_fd = -1; //Make this reentrant safe
    }

    for(ch_word i = 0; i < CAMIO_UDP_STREAM_BUFFERS; i++){
        init_buffer_hdr(this,&priv->rd_buffer_hdrs[i], NULL,0,true);
        if(priv->rd_buffers[i]){
            free(priv->rd_buffers[i]);
            priv->rd_buffers[i] = NULL;
        }

        init_buffer_hdr(this,&priv->wr_buffer_hdrs[i], NULL,0,true);
        if(priv->wr_buffers[i]){
            free(priv->wr_buffers[i]);
            priv->wr_buffers[i] = NULL;
        }
    }
}



camio_error_t udp_stream_construct(camio_stream_t* this, camio_connector_t* connector, int rd_fd, int wr_fd)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    priv->connector = *connector; //Keep a copy of the connector state
    priv->rd_fd = rd_fd;
    priv->wr_fd = wr_fd;

    //Make sure the file descriptors are in non-blocking mode
    int flags = fcntl(priv->rd_fd, F_GETFL, 0);
    fcntl(priv->rd_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(priv->wr_fd, F_GETFL, 0);
    fcntl(priv->wr_fd, F_SETFL, flags | O_NONBLOCK);

    //Make and init the buffers
    for(ch_word i = 0; i < CAMIO_UDP_STREAM_BUFFERS; i++){
        priv->rd_buffers[i] = calloc(1, CAMIO_UDP_BUFFER_SIZE);
        if(!priv->rd_buffers){
            udp_destroy(this);
            return CAMIO_ENOMEM;
        }

        priv->wr_buffers[i] = calloc(1, CAMIO_UDP_BUFFER_SIZE);
        if(!priv->wr_buffers){

            return CAMIO_ENOMEM;
        }

        memset(&priv->rd_buffer_hdrs[i],0,sizeof(camio_buffer_t));
        memset(&priv->wr_buffer_hdrs[i],0,sizeof(camio_buffer_t));

        init_buffer_hdr(this,&priv->rd_buffer_hdrs[i], priv->rd_buffers[i],i,true);
        init_buffer_hdr(this,&priv->wr_buffer_hdrs[i], priv->wr_buffers[i],i,false);
    }


    return CAMIO_ENOERROR;
}





static camio_error_t udp_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o,  ch_word* chain_len_o,
        ch_word buffer_offset, ch_word source_offset)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(buffer_offset != 0){
        return CAMIO_EINVALID; //Not (yet) supported
    }

    if(source_offset != 0){
        return CAMIO_EINVALID; //Not supported
    }

    if( NULL == buffer_chain_o || NULL == *buffer_chain_o){
        return CAMIO_EINVALID;
    }

    if(chain_len_o == NULL){
        return CAMIO_EINVALID;
    }

    if( (*chain_len_o) != 1){
        return CAMIO_EINVALID; //Not (yet) supported
    }

    DBG("Doing read acquire\n");
    ch_word buff_idx = priv->curr_rd_buff;
    if(priv->rd_buffer_hdrs[buff_idx].valid){
        return CAMIO_ENOSLOTS; //Have run out of buffers!
    }
    ch_word bytes = read(priv->rd_fd,priv->rd_buffers[buff_idx], CAMIO_UDP_BUFFER_SIZE);
    if(bytes < 0){
        if(errno == EWOULDBLOCK || errno == EAGAIN){
            return CAMIO_ETRYAGAIN;
        }
        else{
            return CAMIO_ECHECKERRORNO;
        }
    }

    //If we got here, the read was successful, do the internal accounting
    priv->rd_buffer_hdrs[buff_idx].data_start = priv->rd_buffer_hdrs[buff_idx].buffer_start;
    priv->rd_buffer_hdrs[buff_idx].next = NULL;
    priv->rd_buffer_hdrs[buff_idx].orig_len = bytes;
    priv->rd_buffer_hdrs[buff_idx].data_len = bytes;
    priv->rd_buffer_hdrs[buff_idx].valid = true;

    //Prepare the outputs
    *buffer_chain_o = &priv->rd_buffer_hdrs[buff_idx];
    *chain_len_o = 1;

    //Ang get ready for next time
    buff_idx++;
    buff_idx %= CAMIO_UDP_STREAM_BUFFERS;

    return CAMIO_ENOERROR;
}


static camio_error_t udp_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer_chain)
{
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;
    (void)buffer_chain;

    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* count_io)
{
    (void)this;
    (void)buffer_chain_o;
    (void)count_io;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain, ch_word buffer_offset,
        ch_word dest_offset)
{
    (void)this;
    (void)buffer_chain;
    (void)buffer_offset;
    (void)dest_offset;
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t udp_write_release(camio_stream_t* this, camio_wr_buffer_t** buffers_chain)
{
    (void)this;
    (void)buffers_chain;
    return CAMIO_NOTIMPLEMENTED;
}


NEW_STREAM_DEFINE(udp,udp_stream_priv_t)
