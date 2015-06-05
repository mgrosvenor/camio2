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
#include <src/buffers/buffer_malloc_linear.h>

#include "../../camio_debug.h"

#include "udp_stream.h"
#include "udp_buffer.h"

#include <fcntl.h>
#include <errno.h>

typedef struct udp_stream_priv_s {
    camio_connector_t connector;

    int rd_fd;
    int wr_fd;

    //Buffers for data
    buffer_malloc_linear_t* rd_buff;
    buffer_malloc_linear_t* wr_buff;

} udp_stream_priv_t;





static void udp_destroy(camio_stream_t* this)
{
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return;
    }
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);


    if(priv->rd_fd > -1){
        close(priv->rd_fd);
        priv->rd_fd = -1; //Make this reentrant safe
    }

    if(priv->wr_fd > -1){
        close(priv->wr_fd);
        priv->wr_fd = -1; //Make this reentrant safe
    }

}

#define CAMIO_UDP_BUFFER_SIZE (64 * 1024 * 1024)
#define CAMIO_UDP_BUFFER_COUNT (256)
camio_error_t udp_stream_construct(camio_stream_t* this, camio_connector_t* connector, int rd_fd, int wr_fd)
{
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    priv->connector = *connector; //Keep a copy of the connector state
    priv->rd_fd = rd_fd;
    priv->wr_fd = wr_fd;

    //Make sure the file descriptors are in non-blocking mode
    int flags = fcntl(priv->rd_fd, F_GETFL, 0);
    fcntl(priv->rd_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(priv->wr_fd, F_GETFL, 0);
    fcntl(priv->wr_fd, F_SETFL, flags | O_NONBLOCK);

    camio_error_t error = CAMIO_ENOERROR;
    error = buffer_malloc_linear_new(this,CAMIO_UDP_BUFFER_SIZE,CAMIO_UDP_BUFFER_COUNT,true,&priv->rd_buff);
    if(error){
        DBG("No memory for linear read buffer!\n");
        return CAMIO_ENOMEM;
    }
    error = buffer_malloc_linear_new(this,CAMIO_UDP_BUFFER_SIZE,CAMIO_UDP_BUFFER_COUNT,false,&priv->wr_buff);
    if(error){
        DBG("No memory for linear write buffer!\n");
        return CAMIO_ENOMEM;
    }


    DBG("Done constructing UDP stream with read_fd=%i and write_fd=%i\n", rd_fd, wr_fd);
    return CAMIO_ENOERROR;
}





static camio_error_t udp_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_chain_o,  ch_word* chain_len_o,
        ch_word buffer_offset, ch_word source_offset)
{
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    //DBG("buffer_chain = %p &buffer_chain = %p\n", *buffer_chain_o, buffer_chain_o);

    if( NULL == buffer_chain_o){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL != *buffer_chain_o){
        DBG("Buffer chain not null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if(NULL == chain_len_o){
        DBG("Chain len\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( (*chain_len_o) <= 0){ //WTF?
        DBG("WTF? Requesting <= 0 buffers?\n");
        return CAMIO_EINVALID; //Not (yet) supported
    }

    if(source_offset != 0){
        DBG("Src offset\n");
        return CAMIO_EINVALID; //Not supported
    }

    if(buffer_offset != 0){
        DBG("Buff offset\n");
        return CAMIO_EINVALID; //Not (yet) supported
    }

    //DBG("Doing read acquire\n");
    camio_error_t err = buffer_malloc_linear_acquire(priv->rd_buff,buffer_chain_o);
    if(err){ return err; }

    ch_word bytes = read(priv->rd_fd,(*buffer_chain_o)->buffer_start, CAMIO_UDP_BUFFER_SIZE);
    if(bytes < 0){
        buffer_malloc_linear_release(priv->rd_buff,buffer_chain_o);
        *buffer_chain_o = NULL;

        if(errno == EWOULDBLOCK || errno == EAGAIN){
            return CAMIO_ETRYAGAIN;
        }
        else{
            DBG("Check error\n");
            return CAMIO_ECHECKERRORNO;
        }
    }
    (*buffer_chain_o)->data_len = bytes;
    *chain_len_o = 1; //Constant for the moment...
    DBG("Got %lli bytes from UDP read\n", (*buffer_chain_o)->data_len );

    return CAMIO_ENOERROR;
}


static camio_error_t udp_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer_chain)
{
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if( NULL == buffer_chain){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer_chain){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    camio_rd_buffer_t* chain_ptr = *buffer_chain;
    camio_rd_buffer_t* chain_ptr_prev = NULL;
    while(chain_ptr != NULL){
        camio_error_t err = buffer_malloc_linear_release(priv->rd_buff,&chain_ptr);
        if(err){ return err; }
        chain_ptr_prev      = *buffer_chain;
        chain_ptr           = (*buffer_chain)->next;
        chain_ptr->next     = NULL;
    }

    *buffer_chain = NULL; //Remove dangling pointers!

    return CAMIO_ENOERROR;
}


static camio_error_t udp_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_chain_o, ch_word* chain_len_io)
{
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if( NULL == buffer_chain_o){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL != *buffer_chain_o){
        DBG("Buffer chain not null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if(NULL == chain_len_io){
        DBG("Chain len\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( (*chain_len_io) <= 0){ //WTF?
        DBG("WTF? Requesting <= 0 buffers?\n");
        return CAMIO_EINVALID; //Not (yet) supported
    }

    DBG("Doing write acquire\n");
    camio_error_t err = buffer_malloc_linear_acquire(priv->wr_buff,buffer_chain_o);
    if(err){ return err; }
    *chain_len_io = 1; //Constant for the moment...

    return CAMIO_ENOERROR;
}


static camio_error_t udp_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain, ch_word buffer_offset,
        ch_word dest_offset)
{
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)buffer_offset; //Hmmm not sure how to use this....
    (void)dest_offset;


    camio_rd_buffer_t* chain_ptr = *buffer_chain;
    while(chain_ptr != NULL){
        DBG("Writing %li bytes from %p to %i\n", chain_ptr->data_len,chain_ptr->data_start, priv->wr_fd);
        ch_word bytes = write(priv->wr_fd,chain_ptr->data_start,chain_ptr->data_len);
        if(bytes < 0){
            if(errno == EWOULDBLOCK || errno == EAGAIN){
                return CAMIO_ETRYAGAIN;
            }
            else{
                DBG(" error %s\n",strerror(errno));
                return CAMIO_ECHECKERRORNO;
            }
        }
        chain_ptr->data_len -= bytes;
        const char* data_start_new = (char*)chain_ptr->data_start + bytes;
        chain_ptr->data_start = (void*)data_start_new;

        chain_ptr = chain_ptr->next;
    }

    return CAMIO_ENOERROR;
}


static camio_error_t udp_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer_chain)
{
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    udp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if( NULL == buffer_chain){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer_chain){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    camio_rd_buffer_t* chain_ptr = *buffer_chain;
    camio_rd_buffer_t* chain_ptr_prev = NULL;
    while(chain_ptr != NULL){
        camio_error_t err = buffer_malloc_linear_release(priv->wr_buff,&chain_ptr);
        if(err){ return err; }
        chain_ptr_prev      = *buffer_chain;
        chain_ptr           = (*buffer_chain)->next;
        chain_ptr->next     = NULL;
    }

    *buffer_chain = NULL; //Remove dangling pointers!

    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(udp,udp_stream_priv_t)
