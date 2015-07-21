/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   01 Jul 2015
 *  File name: mfio_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <src/transports/stream.h>
#include <src/transports/connector.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>

#include <src/camio_debug.h>

#include "mfio_stream.h"

#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <memory.h>


/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct mfio_stream_priv_s {
    camio_controller_t connector;

    //To manage read buffer/requests --TODO should harmonize the naming convention. read vs rd etc
    ch_bool is_rd_closed;
    camio_buffer_t mmap_rd_buff;
    ch_bool read_registered;    //Has a read been registered?
    ch_bool read_ready;         //Is the stream ready for reading? (for edge triggered multiplexers)
    camio_read_req_t* read_req;  //Read request vector to put data into when there is new data
    ch_word read_req_len;       //size of the request vector
    ch_word read_req_curr;      //This will be needed later

    //To manage write buffer/requests
    ch_bool is_wr_closed;
    camio_buffer_t mmap_wr_buff;
    ch_bool write_acquired;       //Has a write alreay been acquired?
    ch_bool write_registered;     //Has a write been registered?
    ch_bool write_ready;          //Is the stream ready for writing (for edge triggered multiplexers)
    camio_write_req_t* write_req;  //Write request vector to take data out of when there is new data.
    ch_word write_req_len;        //size of the request vector
    ch_word write_req_curr;

} mfio_stream_priv_t;


/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/
static void mfio_read_close(camio_stream_t* this){
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if(!priv->is_rd_closed){
        priv->mmap_rd_buff.data_start = NULL;
        priv->mmap_rd_buff.data_len   = 0;
        priv->is_rd_closed             = true;
    }
}

//See if there is something to read
static camio_error_t mfio_read_peek( camio_stream_t* this)
{
    //This is not a public function, can assume that preconditions have been checked.
    DBG("Doing read peek\n");
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->is_rd_closed){
        DBG("Cannot keep reading, the stream is closed\n");
        return CAMIO_ECLOSED;
    }

    camio_read_req_t* req = &priv->read_req[0]; //Assume that req_len == 1

    //WARNING: Code below here assumes that req len == 1!!
    if( req->src_offset_hint != CAMIO_READ_REQ_SRC_OFFSET_NONE){
        DBG("This stream currently does no support source offsets!\n");
        //Fundamental question. If you support arbitrary offsets, then the stream has infinite length and will never close.
        //is this ok? Is it a problem? If you do an offset read, surely all reads should be offset reads? This suggests that
        //this stream can be in one of two modes. 1) auto increment mode and 2) offset mode and that these cannot be mixed.
        return CAMIO_EINVALID;
    }
    if( req->dst_offset_hint != CAMIO_READ_REQ_DST_OFFSET_NONE){
        DBG("This stream does not support destination offsets. (It is a buffer donor!)\n");
        return CAMIO_EINVALID;
    }
    if(req->read_size_hint == CAMIO_READ_REQ_SIZE_ANY){
        req->read_size_hint = priv->mmap_rd_buff.__internal.__mem_len;
    }
    if(req->read_size_hint > priv->mmap_rd_buff.__internal.__mem_len){
        //TODO XXX: Could realloc buffer here to be the right size...
        req->read_size_hint = priv->mmap_rd_buff.__internal.__mem_len;
    }

    DBG("mem_start=%p, data_start=%p, mem_len=%lli, data_len=%lli\n",
            priv->mmap_rd_buff.__internal.__mem_start,
            priv->mmap_rd_buff.data_start,
            priv->mmap_rd_buff.__internal.__mem_len,
            priv->mmap_rd_buff.data_len
            );


    const ch_word current_offset = (char*)priv->mmap_rd_buff.data_start - (char*)priv->mmap_rd_buff.__internal.__mem_start;
    const ch_word max_read_size  = priv->mmap_rd_buff.__internal.__mem_len - current_offset;
    ch_word read_size = MIN(max_read_size,req->read_size_hint);

    //Check if we're at the end of the buffer
    if( current_offset >= priv->mmap_rd_buff.__internal.__mem_len  || priv->is_rd_closed){
        mfio_read_close(this);
        priv->read_ready = true;
        DBG("Have run out of data. Cannot keep reading, the stream is closed\n");
        return CAMIO_ENOERROR; //Nothing more to read
    }

    priv->mmap_rd_buff.data_len   = read_size;
    priv->read_ready              = true;

    DBG("Success - got %lli bytes from MFIO peek\n", priv->mmap_rd_buff.data_len );
    return CAMIO_ENOERROR;
}


static camio_error_t mfio_read_ready(camio_muxable_t* this)
{
    DBG("Doing MFIO ready...\n");

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
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);

    if(!priv->read_registered){ //Even if there is data waiting, nobody want's it, so ignore for now.
        DBG("Nobody wants the data\n");
        return CAMIO_ENOTREADY;
    }

    //Check if there is data already waiting to be dispatched
    if(priv->read_ready){
        DBG("There is data already waiting!\n");
        return CAMIO_EREADY;
    }

    //Nope, OK, see if we can get some
    camio_error_t err = mfio_read_peek(this->parent.stream);
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

static camio_error_t mfio_read_request( camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len )
{
    DBG("Doing MFIO read request...!\n");
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

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

    DBG("Doing MFIO read request...Done!..Successful\n");
    return CAMIO_ENOERROR;

}


static camio_error_t mfio_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
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
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    camio_error_t err = mfio_read_ready(&this->rd_muxable);
    if(err == CAMIO_EREADY){ //Whoo hoo! There's data!
        *buffer_o = &priv->mmap_rd_buff;
        DBG("Got new MFIO data of size %i\n", priv->mmap_rd_buff.data_len);
        return CAMIO_ENOERROR;
    }

    if(err == CAMIO_ENOTREADY){
        return CAMIO_ETRYAGAIN;
    }

    //Ugh don't know what went wrong.
    DBG("Something bad happened!\n");
    return err;
}


static camio_error_t mfio_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

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

    char* new_data_start  = (char*)priv->mmap_rd_buff.data_start + priv->mmap_rd_buff.data_len; //TODO BUG! Assumes that user has not played with this value...
    priv->mmap_rd_buff.data_start = new_data_start;

    *buffer               = NULL; //Remove dangling pointers!
    priv->read_registered = false; //unregister the outstanding read. And now we're done.
    priv->read_ready      = false; //We're we've consumed the data now.

    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS
 **************************************************************************************************************************/
static void mfio_write_close(camio_stream_t* this){
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if(!priv->is_wr_closed){
        priv->mmap_wr_buff.data_start = NULL;
        priv->mmap_wr_buff.data_len   = 0;
        priv->is_wr_closed = true;
    }
}


static camio_error_t mfio_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    DBG("Doing write acquire\n");

    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if( NULL == buffer_o){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL != *buffer_o){
        DBG("Buffer chain not null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( priv->write_acquired){
        DBG("Write buffer already acquired. Can only do this once\n");
        return CAMIO_ENOBUFFS;
    }

    //Check if we're at the end of the buffer
    const ch_word current_offset = (char*)priv->mmap_rd_buff.data_start - (char*)priv->mmap_rd_buff.__internal.__mem_start;
    if( current_offset >= priv->mmap_wr_buff.__internal.__mem_len  || priv->is_wr_closed){
        mfio_write_close(this);
        DBG("Have run out of data. Cannot keep reading, the stream is closed\n");
        return CAMIO_ECLOSED; //Nothing more to read
    }

    *buffer_o = &priv->mmap_wr_buff;
    priv->write_acquired = true;
    DBG("Returning new buffer of size %lli at %p\n", (*buffer_o)->data_start, (*buffer_o)->data_len);
    return CAMIO_ENOERROR;
}


static camio_error_t mfio_write_request(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(req_vec_len != 1){
        DBG("request length of %lli requested. This value is not currently supported\n", req_vec_len);
        return CAMIO_NOTIMPLEMENTED;
    }

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
static camio_error_t mfio_write_try(camio_stream_t* this)
{
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    camio_write_req_t* req = &priv->write_req[0];

    if(req->buffer->__internal.__parent != this){
        //TODO XXX could add this feature but it would be non-trivial
        DBG("This stream cannot support writing from an alternative source with zero copy\n");
        return CAMIO_EINVALID;
    }

    if( req->src_offset_hint != CAMIO_WRITE_REQ_SRC_OFFSET_NONE){
        DBG("This stream currently does no support source offsets!\n");
        //Fundamental question. If you support arbitrary offsets, then the stream has infinite length and will never close.
        //is this ok? Is it a problem? If you do an offset read, surely all reads should be offset reads? This suggests that
        //this stream can be in one of two modes. 1) auto increment mode and 2) offset mode and that these cannot be mixed.
        return CAMIO_EINVALID;
    }
    if( req->dst_offset_hint != CAMIO_WRITE_REQ_DST_OFFSET_NONE){
        DBG("This stream does not support source offsets. (It is a buffer donor!)\n");
        return CAMIO_EINVALID;
    }
    if( req->buffer->data_len > req->buffer->__internal.__mem_len){
        DBG("Warning: It's looks like you've overwritten the length of the underlying memory segment. "
                "Data corruption is likely to have happened\n");
        return CAMIO_ETOOMANY;
    }

    camio_buffer_t* buff   = req->buffer;
    DBG("Trying to write %li bytes from %pi\n", buff->data_len,buff->data_start);
    const char* data_start_new = (char*)buff->data_start + buff->data_len;
    buff->data_start = (void*)data_start_new;
    buff->data_len = buff->__internal.__mem_len - buff->data_len;

    //If we get here, we've successfully written all the records out. This means we're now ready to take more write requests
    DBG("Deregistering write\n");
    priv->write_registered = false;

    return CAMIO_ENOERROR;
}



//Is the underlying stream done writing and ready for more?
static camio_error_t mfio_write_ready(camio_muxable_t* this)
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

    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);
    if(!priv->write_registered){ //No body has asked us to write anything, so we're not ready
        return CAMIO_ENOTREADY;
    }

    //OK now the fun begins
    camio_error_t err = mfio_write_try(this->parent.stream);
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


static camio_error_t mfio_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
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


    DBG("Releaseing data buffer %p staring at %p\n", (*buffer), (*buffer)->__internal.__mem_start);

    priv->write_acquired = false;

    *buffer = NULL; //Remove dangling pointers!

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * SETUP/CLEANUP FUNCTIONS
 **************************************************************************************************************************/
static void mfio_destroy(camio_stream_t* this)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return;
    }
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->mmap_rd_buff.__internal.__mem_start){
        munmap(priv->mmap_rd_buff.__internal.__mem_start, priv->mmap_rd_buff.__internal.__mem_len);
        mfio_read_close(this);
        mfio_write_close(this);
    }

    if(priv->mmap_wr_buff.__internal.__mem_start){
        munmap(priv->mmap_wr_buff.__internal.__mem_start, priv->mmap_wr_buff.__internal.__mem_len);
        mfio_read_close(this);
        mfio_write_close(this);
    }

    if(this->rd_muxable.fd > -1){
        close(this->rd_muxable.fd);
        this->rd_muxable.fd = -1;
        this->wr_muxable.fd = -1;
    }

    free(this);
}

void init_buffer(camio_buffer_t* buff, ch_bool read_only, camio_stream_t* parent, void* base_mem_start, ch_word base_mem_len)
{
    buff->__internal.__mem_start        = base_mem_start;
    buff->__internal.__mem_len          = base_mem_len;
    buff->__internal.__buffer_id        = 0;
    buff->__internal.__do_auto_release  = false;
    buff->__internal.__in_use           = false;
    buff->__internal.__parent           = parent;
    buff->__internal.__pool_id          = 0;
    buff->__internal.__read_only        = read_only;

    buff->data_start = buff->__internal.__mem_start;
    buff->data_len   = buff->__internal.__mem_len;

}


camio_error_t mfio_stream_construct(
    camio_stream_t* this,
    camio_controller_t* connector,
    int base_fd,
    void* base_mem_start,
    ch_word base_mem_len
)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    mfio_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    priv->connector = *connector; //Keep a copy of the connector state
    //Set up the buffer states
    init_buffer(&priv->mmap_rd_buff,true,this, base_mem_start, base_mem_len);
    init_buffer(&priv->mmap_wr_buff,false,this, base_mem_start, base_mem_len);

    this->rd_muxable.mode              = CAMIO_MUX_MODE_READ;
    this->rd_muxable.parent.stream     = this;
    this->rd_muxable.vtable.ready      = mfio_read_ready;
    this->rd_muxable.fd                = base_fd;

    this->wr_muxable.mode              = CAMIO_MUX_MODE_WRITE;
    this->wr_muxable.parent.stream     = this;
    this->wr_muxable.vtable.ready      = mfio_write_ready;
    this->wr_muxable.fd                = base_fd;


    DBG("Done constructing MFIO stream with size=%lli\n", priv->mmap_rd_buff.__internal.__mem_len);
    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(mfio,mfio_stream_priv_t)
