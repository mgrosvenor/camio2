/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   22 Jun 2015
 *  File name: tcp_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include <src/transports/stream.h>
#include <src/transports/connector.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>

#include "../../camio_debug.h"

#include "tcp_stream.h"

#include <fcntl.h>
#include <errno.h>

/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
#define CAMIO_TCP_BUFFER_SIZE (4 * 1024 * 1024)
//Current (simple) TCP recv only does one buffer at a time, this could change with vectored I/O in the future.
#define CAMIO_TCP_BUFFER_COUNT (1)

typedef struct tcp_stream_priv_s {
    camio_connector_t connector;

    //The actual underlying file descriptor that we're going to work with. Default is -1
    int fd;

    //Buffer pools for data -- This is really a place holder for vectored I/O in the future
    buffer_malloc_linear_t* rd_buff_pool;
    buffer_malloc_linear_t* wr_buff_pool;

    //The current read buffer
    ch_bool read_registered;    //Has a read been registered?
    ch_bool read_ready;         //Is the stream ready for writing (for edge triggered multiplexers)
    camio_buffer_t* rd_buffer;  //A place to keep a read buffer until it's ready to be consumed
    camio_read_req_t* read_req; //Read request vector to put data into when there is new data
    ch_word read_req_len;       //size of the request vector
    ch_word read_req_curr;      //This will be needed later

    ch_bool write_registered;     //Has a write been registered?
    ch_bool write_ready;          //Is the stream ready for writing (for edge triggered multiplexers)
    camio_write_req_t* write_req; //Write request vector to take data out of when there is new data.
    ch_word write_req_len;        //size of the request vector
    ch_word write_req_curr;

} tcp_stream_priv_t;




/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/

//See if there is something to read
static camio_error_t tcp_read_peek( camio_stream_t* this)
{
    //This is not a public function, can assume that preconditions have been checked.
    //DBG("Doing read peek\n");
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    camio_error_t err = buffer_malloc_linear_acquire(priv->rd_buff_pool,&priv->rd_buffer);
    if(err){
        DBG("Could not acquire read buffer Have you called release?!\n");
        return err;
    }

    camio_read_req_t * req = priv->read_req;
    req->dst_offset_hint = MIN(CAMIO_TCP_BUFFER_SIZE, req->dst_offset_hint); //Make sure we don't overflow the buffer
    char* read_buffer = (char*)priv->rd_buffer->buffer_start + req->dst_offset_hint; //Do the offset that we need
    ch_word read_size = CAMIO_TCP_BUFFER_SIZE - req->dst_offset_hint; //Also make sure we don't overflow
    if(req->read_size_hint){
        read_size = MIN(read_size,req->read_size_hint);
    }
    ch_word bytes = read(priv->fd, read_buffer, read_size);
    if(bytes < 0){ //Shit, got an error. Maybe there just isn't any data?
        buffer_malloc_linear_release(priv->rd_buff_pool,&priv->rd_buffer); //TODO, could remove this step and reuse buffer..
        priv->rd_buffer = NULL;

        if(errno == EWOULDBLOCK || errno == EAGAIN){
            return CAMIO_ETRYAGAIN;
        }
        else{
            DBG("Something else went wrong, check errno value\n");
            return CAMIO_ECHECKERRORNO;
        }
    }

    priv->rd_buffer->data_len = bytes;
    priv->rd_buffer->data_start = read_buffer;
    DBG("Got %lli bytes from TCP peek\n", priv->rd_buffer->data_len );

    return CAMIO_ENOERROR;
}


static camio_error_t tcp_read_ready(camio_muxable_t* this)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( this->mode != CAMIO_MUX_MODE_READ){
        DBG("Wrong kind of muxable!\n"); //WTF??
        return CAMIO_EINVALID;
    }

    //OK now the fun begins
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);

    if(!priv->read_registered){ //Even if there is data waiting, nobody want's it, so ignore for now.
        DBG("Nobody wants the data\n");
        return CAMIO_ENOTREADY;
    }

    //Check if there is data already waiting to be dispatched
    if(priv->rd_buffer){
        DBG("There is data already waiting!\n");
        return CAMIO_EREADY;
    }

    //Nope, OK, see if we can get some
    camio_error_t err = tcp_read_peek(this->parent.stream);
    if(err == CAMIO_ENOERROR ){
        DBG("There is new data waiting!\n");
        return CAMIO_EREADY;
    }

    if(err == CAMIO_ETRYAGAIN){
        //DBG("This is no new data waiting!\n");
        return CAMIO_ENOTREADY;
    }

    DBG("Something bad happened!\n");
    return err;


}

static camio_error_t tcp_read_request( camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len )
{
    DBG("Doing TCP read register...!\n");
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if(req_vec_len != 1){
        DBG("Stream currently only supports read requests of size 1\n"); //TODO this should be coded in the features struct
        return CAMIO_EINVALID;
    }

    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if( req_vec->src_offset_hint != 0){
        DBG("This is TCP, you're not allowed to have a source offset!\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( req_vec->dst_offset_hint > CAMIO_TCP_BUFFER_SIZE){
        DBG("You're trying to set a buffer offset (%i) which is greater than the buffer size (%i) ???\n",
                req_vec->dst_offset_hint, CAMIO_TCP_BUFFER_SIZE); //WTF?
        return CAMIO_EINVALID;
    }

    if(priv->read_registered){
        DBG("Already registered a read request. Currently this stream only handles one outstanding request at a time\n");
        return CAMIO_ETOOMANY; //TODO XXX better error code
    }

    if(req_vec->read_size_hint > 0){
        if(req_vec->read_size_hint > CAMIO_TCP_BUFFER_SIZE){
            //TODO XXX: Could realloc buffer here to be the right size...
            req_vec->read_size_hint = CAMIO_TCP_BUFFER_SIZE;
        }
    }

    //Sanity checks done, do some work now
    priv->read_registered = true;

    DBG("Doing TCP read register...Done!..Successful\n");
    return CAMIO_ENOERROR;

}


static camio_error_t tcp_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
{

    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( NULL == buffer_o){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( NULL != *buffer_o){
        DBG("Buffer chain not null. You should release this before getting a new one, otherwise dangling pointers!\n");
        return CAMIO_EINVALID;
    }
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    camio_error_t err = tcp_read_ready(&this->rd_muxable);
    if(err == CAMIO_EREADY){ //Whoo hoo! There's data!
        *buffer_o = priv->rd_buffer;
        DBG("Got new TCP data of size %i\n", priv->rd_buffer->data_len);
        return CAMIO_ENOERROR;
    }

    if(err == CAMIO_ENOTREADY){
        return CAMIO_ETRYAGAIN;
    }

    //Ugh don't know what went wrong.
    DBG("Something bad happened!\n");
    return err;
}


static camio_error_t tcp_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    if( NULL == buffer){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    camio_error_t err = buffer_malloc_linear_release(priv->rd_buff_pool,buffer);
    if(err){ return err; }

    *buffer               = NULL; //Remove dangling pointers!
    priv->rd_buffer       = NULL; //Remove local reference to the buffer
    priv->read_registered = false; //unregister the outstanding read. And now we're done.

    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS
 **************************************************************************************************************************/

static camio_error_t tcp_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if( NULL == buffer_o){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL != *buffer_o){
        DBG("Buffer chain not null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    DBG("Doing write acquire\n");
    camio_error_t err = buffer_malloc_linear_acquire(priv->wr_buff_pool,buffer_o);
    if(err){ return err; }

    DBG("Returning new buffer of size %lli at %p\n", (*buffer_o)->buffer_len, (*buffer_o)->buffer_start);
    return CAMIO_ENOERROR;
}


static camio_error_t tcp_write_request(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    DBG("Got a write request vector with %lli items\n", req_vec_len);
    if(priv->write_registered){
        DBG("Already registered a write request. Currently this stream only handles one outstanding request at a time\n");
        return CAMIO_EINVALID; //TODO XXX better error code
    }

    //Register the new write request
    priv->write_req         = req_vec;
    priv->write_req_len     = req_vec_len;
    priv->write_registered  = true;
    priv->write_req_curr    = 0;

    return CAMIO_ENOERROR;
}


//Try to write to the underlying, stream. This function is private, so no precondition checks necessary
static camio_error_t tcp_write_try(camio_stream_t* this)
{
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    for(int i = priv->write_req_curr; i < priv->write_req_len; i++){
        camio_write_req_t* req = priv->write_req + i;
        camio_buffer_t* buff   = req->buffer;
        DBG("Trying to writing %li bytes from %p to %i\n", buff->data_len,buff->data_start, priv->fd);
        ch_word bytes = write(priv->fd,buff->data_start,buff->data_len);
        if(bytes < 0){
            if(errno == EWOULDBLOCK || errno == EAGAIN){
                return CAMIO_ETRYAGAIN;
            }
            else{
                DBG("error %s\n",strerror(errno));
                return CAMIO_ECHECKERRORNO;
            }
        }

        buff->data_len -= bytes;
        const char* data_start_new = (char*)buff->data_start + bytes;
        buff->data_start = (void*)data_start_new;

        if(buff->data_len != 0){
            return CAMIO_ETRYAGAIN;
        }

    }

    //If we get here, we've successfully written all the records out. This means we're now ready to take more write requests
    priv->write_registered = false;

    return CAMIO_ENOERROR;
}



//Is the underlying stream done writing and ready for more?
static camio_error_t tcp_write_ready(camio_muxable_t* this)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( this->mode != CAMIO_MUX_MODE_WRITE){
        DBG("Wrong kind of muxable!\n"); //WTF??
        return CAMIO_EINVALID;
    }

    //OK now the fun begins
    camio_error_t err = tcp_write_try(this->parent.stream);
    if(err == CAMIO_ENOERROR){
        DBG("Writing has completed successfully. Release the buffers and/or write some more\n");
        return CAMIO_EREADY;
    }

    if(err == CAMIO_ETRYAGAIN){
        DBG("Writing not yet complete, come back later and try again\n");
        return CAMIO_ENOTREADY;
    }

    if(err == CAMIO_ECLOSED){
        DBG("The stream has been closed. You cannot write any more\n");
        return CAMIO_ECLOSED;
    }

    DBG("Ouch, something bad happened. Unexpected err = %lli \n", err);
    return err;

}


static camio_error_t tcp_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    if( NULL == buffer){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    DBG("Removing data buffer %p staring at %p\n", buffer, (*buffer)->buffer_start);

    *buffer = NULL; //Remove dangling pointers!

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * SETUP/CLEANUP FUNCTIONS
 **************************************************************************************************************************/

static void tcp_destroy(camio_stream_t* this)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return;
    }
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->fd > -1){
        close(priv->fd);
        priv->fd = -1; //Make this reentrant safe
    }


    if(priv->rd_buff_pool){
        buffer_malloc_linear_destroy(&priv->rd_buff_pool);
    }

    if(priv->wr_buff_pool){
        buffer_malloc_linear_destroy(&priv->wr_buff_pool);
    }

    free(this);

}

camio_error_t tcp_stream_construct(camio_stream_t* this, camio_connector_t* connector, int fd)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    tcp_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    priv->connector = *connector; //Keep a copy of the connector state
    priv->fd = fd;

    //Make sure the file descriptors are in non-blocking mode
    int flags = fcntl(priv->fd, F_GETFL, 0);
    fcntl(priv->fd, F_SETFL, flags | O_NONBLOCK);

    camio_error_t error = CAMIO_ENOERROR;
    error = buffer_malloc_linear_new(this,CAMIO_TCP_BUFFER_SIZE,CAMIO_TCP_BUFFER_COUNT,true,&priv->rd_buff_pool);
    if(error){
        DBG("No memory for linear read buffer!\n");
        return CAMIO_ENOMEM;
    }
    error = buffer_malloc_linear_new(this,CAMIO_TCP_BUFFER_SIZE,CAMIO_TCP_BUFFER_COUNT,false,&priv->wr_buff_pool);
    if(error){
        DBG("No memory for linear write buffer!\n");
        return CAMIO_ENOMEM;
    }

    this->rd_muxable.mode              = CAMIO_MUX_MODE_READ;
    this->rd_muxable.parent.stream     = this;
    this->rd_muxable.vtable.ready      = tcp_read_ready;
    this->rd_muxable.fd                = priv->fd;

    this->wr_muxable.mode              = CAMIO_MUX_MODE_WRITE;
    this->wr_muxable.parent.stream     = this;
    this->wr_muxable.vtable.ready      = tcp_write_ready;
    this->wr_muxable.fd                = priv->fd;


    DBG("Done constructing TCP stream with read_fd=%i and write_fd=%i\n", fd, fd);
    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(tcp,tcp_stream_priv_t)
