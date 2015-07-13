/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   03 Jul 2015
 *  File name: fio_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#include <src/transports/stream.h>
#include <src/transports/connector.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>

#include <src/camio_debug.h>

#include "fio_stream.h"

#include <fcntl.h>
#include <errno.h>

/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
//Current (simple) FIO recv only does one buffer at a time, this could change with vectored I/O in the future.
#define CAMIO_FIO_BUFFER_COUNT (1)

typedef struct fio_stream_priv_s {
    camio_connector_t connector;

    //The actual underlying file descriptors that we're going to work with. Default is -1
    int rd_fd;
    int wr_fd;
    ch_word rd_buff_sz;
    ch_word wr_buff_sz;

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

} fio_stream_priv_t;




/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/

//See if there is something to read
static camio_error_t fio_read_peek( camio_stream_t* this)
{
    //This is not a public function, can assume that preconditions have been checked.
    //DBG("Doing read peek\n");
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    camio_error_t err = buffer_malloc_linear_acquire(priv->rd_buff_pool,&priv->rd_buffer);
    if(err){
        DBG("Could not acquire read buffer Have you called release?!\n");
        return err;
    }

    //TODO XXX - Should pick the right request vec here
    camio_read_req_t* req = priv->read_req;

    //WARNING: Code below here assumes that req len == 1!!
    if( req->src_offset_hint != CAMIO_READ_REQ_SRC_OFFSET_NONE){
        DBG("This is FIO, you're not allowed to have a source offset!\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( req->dst_offset_hint > priv->rd_buff_sz){
        DBG("You're trying to set a buffer offset (%i) which is greater than the buffer size (%i) ???\n",
                req->dst_offset_hint, priv->rd_buff_sz); //WTF?
        return CAMIO_EINVALID;
    }
    if(req->read_size_hint == CAMIO_READ_REQ_SIZE_ANY){
        req->read_size_hint = priv->rd_buff_sz;
    }
    if(req->read_size_hint > priv->rd_buff_sz){
        //TODO XXX: Could realloc buffer here to be the right size...
        req->read_size_hint = priv->rd_buff_sz;
    }

    req->dst_offset_hint = MIN(priv->rd_buff_sz, req->dst_offset_hint); //Make sure we don't overflow the buffer
    char* read_buffer = (char*)priv->rd_buffer->__internal.__mem_start + req->dst_offset_hint; //Do the offset that we need
    ch_word read_size = priv->rd_buff_sz - req->dst_offset_hint; //Also make sure we don't overflow
    read_size = MIN(read_size,req->read_size_hint);

    ch_word bytes = read(priv->rd_fd, read_buffer, read_size);
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
    DBG("Got %lli bytes from FIO peek\n", priv->rd_buffer->data_len );

    return CAMIO_ENOERROR;
}


static camio_error_t fio_read_ready(camio_muxable_t* this)
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
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);

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
    camio_error_t err = fio_read_peek(this->parent.stream);
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

static camio_error_t fio_read_request( camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len )
{
    DBG("Doing FIO read request...!\n");
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(req_vec_len != 1){
        DBG("Stream currently only supports read requests of size 1\n"); //TODO this should be coded in the features struct
        return CAMIO_EINVALID;
    }

    if(priv->read_registered){
        DBG("Already registered a read request. Currently this stream only handles one outstanding request at a time\n");
        return CAMIO_ETOOMANY; //TODO XXX better error code
    }

    //Sanity checks done, do some work now
    priv->read_req          = req_vec;
    priv->read_req_len      = req_vec_len;
    priv->read_registered   = true;

    DBG("Doing FIO read request...Done!..Successful\n");
    return CAMIO_ENOERROR;

}


static camio_error_t fio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
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
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    camio_error_t err = fio_read_ready(&this->rd_muxable);
    if(err == CAMIO_EREADY){ //Whoo hoo! There's data!
        *buffer_o = priv->rd_buffer;
        DBG("Got new FIO data of size %i\n", priv->rd_buffer->data_len);
        return CAMIO_ENOERROR;
    }

    if(err == CAMIO_ENOTREADY){
        return CAMIO_ETRYAGAIN;
    }

    //Ugh don't know what went wrong.
    DBG("Something bad happened!\n");
    return err;
}


static camio_error_t fio_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if( NULL == buffer){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if((*buffer)->__internal.__parent != this){
        //TODO XXX could add this feature but it would be non-trivial
        DBG("Cannot release a buffer that does not belong to us!\n");
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

static camio_error_t fio_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

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

    (*buffer_o)->data_start = (*buffer_o)->__internal.__mem_start;
    (*buffer_o)->data_len = (*buffer_o)->__internal.__mem_len;

    DBG("Returning new buffer of size %lli at %p with parent=%p\n",
        (*buffer_o)->__internal.__mem_len,
        (*buffer_o)->__internal.__mem_start,
        (*buffer_o)->__internal.__parent
    );


    return CAMIO_ENOERROR;
}


static camio_error_t fio_write_request(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

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
static camio_error_t fio_write_try(camio_stream_t* this)
{
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    for(int i = priv->write_req_curr; i < priv->write_req_len; i++){
        camio_write_req_t* req = priv->write_req + i;
        camio_buffer_t* buff   = req->buffer;

        if(req->buffer->__internal.__parent != this){
            ERR("Warning -- detected request to write from buffer with parent %p not belonging to this stream %p\n",
                    req->buffer->__internal.__parent,
                    this
            );
        }

        DBG("Current request = %i - Trying to write %lli bytes from %p to %i\n", i, buff->data_len,buff->data_start, priv->wr_fd);
        ch_word bytes = write(priv->wr_fd,buff->data_start,buff->data_len);
        if(bytes < 0){
            if(errno == EWOULDBLOCK || errno == EAGAIN){
                DBG("Stream would block\n");
                return CAMIO_ETRYAGAIN;
            }
            else{
                DBG("error %s\n",strerror(errno));
                return CAMIO_ECHECKERRORNO;
            }
        }
        if(bytes < buff->data_len){
            DBG("Did not write everything, %lli bytes remaining\n", buff->data_len - bytes);
            buff->data_len -= bytes;
            const char* data_start_new = (char*)buff->data_start + bytes;
            buff->data_start = (void*)data_start_new;
            return CAMIO_ETRYAGAIN;
        }


        //Reset everything
        buff->data_start = buff->__internal.__mem_start;
        buff->data_len = buff->__internal.__mem_len;
        DBG("Finished writing everything for request %i\n", i);

    }

    //If we get here, we've successfully written all the records out. This means we're now ready to take more write requests
    DBG("De registering write\n");
    priv->write_registered = false;



    return CAMIO_ENOERROR;
}



//Is the underlying stream done writing and ready for more?
static camio_error_t fio_write_ready(camio_muxable_t* this)
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

    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);
    if(!priv->write_registered){ //No body has asked us to write anything, so we're not ready
        return CAMIO_ENOTREADY;
    }

    //OK now the fun begins
    camio_error_t err = fio_write_try(this->parent.stream);
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


static camio_error_t fio_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    if( NULL == buffer){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if((*buffer)->__internal.__parent != this){
        //TODO XXX could add this feature but it would be non-trivial
        DBG("Cannot release a buffer that does not belong to us!\n");
        return CAMIO_EINVALID;
    }


    DBG("Removing data buffer %p staring at %p\n", buffer, (*buffer)->__internal.__mem_start);
    camio_error_t err = buffer_malloc_linear_release(priv->wr_buff_pool,buffer);
    if(err){
        DBG("Unexpected release error %lli\n", err);
        return err;
    }

    *buffer = NULL; //Remove dangling pointers!

    return CAMIO_ENOERROR;
}



/**************************************************************************************************************************
 * SETUP/CLEANUP FUNCTIONS
 **************************************************************************************************************************/

static void fio_destroy(camio_stream_t* this)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return;
    }
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->rd_fd > -1){
        close(priv->rd_fd);
        priv->rd_fd = -1; //Make this reentrant safe
    }

    if(priv->wr_fd > -1){
        close(priv->wr_fd);
        priv->wr_fd = -1; //Make this reentrant safe
    }

    if(priv->rd_buff_pool){
        buffer_malloc_linear_destroy(&priv->rd_buff_pool);
    }

    if(priv->wr_buff_pool){
        buffer_malloc_linear_destroy(&priv->wr_buff_pool);
    }

    free(this);

}

camio_error_t fio_stream_construct(camio_stream_t* this, camio_connector_t* connector, fio_params_t* params, int rd_fd, int wr_fd)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    fio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    priv->connector     = *connector; //Keep a copy of the connector state
    priv->rd_fd         = rd_fd;
    priv->wr_fd         = wr_fd;
    priv->rd_buff_sz    = params->rd_buff_sz;
    priv->wr_buff_sz    = params->wr_buff_sz;

    //Make sure the file descriptors are in non-blocking mode
    int flags = fcntl(priv->rd_fd, F_GETFL, 0);
    fcntl(priv->rd_fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(priv->wr_fd, F_GETFL, 0);
    fcntl(priv->wr_fd, F_SETFL, flags | O_NONBLOCK);

    camio_error_t error = CAMIO_ENOERROR;
    error = buffer_malloc_linear_new(this,priv->rd_buff_sz,CAMIO_FIO_BUFFER_COUNT,true,&priv->rd_buff_pool);
    if(error){
        DBG("No memory for linear read buffer!\n");
        return CAMIO_ENOMEM;
    }
    error = buffer_malloc_linear_new(this,priv->wr_buff_sz,CAMIO_FIO_BUFFER_COUNT,false,&priv->wr_buff_pool);
    if(error){
        DBG("No memory for linear write buffer!\n");
        return CAMIO_ENOMEM;
    }

    this->rd_muxable.mode              = CAMIO_MUX_MODE_READ;
    this->rd_muxable.parent.stream     = this;
    this->rd_muxable.vtable.ready      = fio_read_ready;
    this->rd_muxable.fd                = priv->rd_fd;

    this->wr_muxable.mode              = CAMIO_MUX_MODE_WRITE;
    this->wr_muxable.parent.stream     = this;
    this->wr_muxable.vtable.ready      = fio_write_ready;
    this->wr_muxable.fd                = priv->wr_fd;


    DBG("Done constructing FIO stream with read_fd=%i and write_fd=%i\n", priv->rd_fd, priv->wr_fd);
    return CAMIO_ENOERROR;
}

NEW_STREAM_DEFINE(fio,fio_stream_priv_t)
