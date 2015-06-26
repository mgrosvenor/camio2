/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include <src/transports/stream.h>
#include <src/transports/connector.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>

#include "../../camio_debug.h"

#include "delim_stream.h"

#include <fcntl.h>
#include <errno.h>

/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct delim_stream_priv_s {
    camio_stream_t* base; //Base stream that will be "decorated" by this delimiter

    //The working buffer is where we do stuff until there is some kind of output ready
    camio_buffer_t work_rd_buff;

    //The result buffer is where we put stuff when there is a result to send out to the outside world
    camio_buffer_t* result_rd_buff;

    //The base buffer is used to gather data from the base stream
    camio_buffer_t* base_rd_buff;
    ch_word base_rd_buff_offset;     //Where should data be placed in the read buffer
    ch_word base_rd_src_offset;   //Where should data be placed in the read buffer
    ch_bool base_rd_registered;

    ch_bool is_rd_closed;

    //    ch_bool read_registered;    //Has a read been registered? -- is this necessary?
    //camio_buffer_t* rd_buffer;  //A place to keep a read buffer until it's ready to be consumed

    int (*delim_fn)(char* buffer, int len);

} delim_stream_priv_t;




/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/
static void delim_read_close(camio_stream_t* this){
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if(!priv->is_rd_closed){
        free(priv->work_rd_buff.buffer_start);
        priv->work_rd_buff.buffer_start = NULL;
        priv->work_rd_buff.buffer_len   = 0;
        priv->work_rd_buff.data_start   = NULL;
        priv->work_rd_buff.data_len     = 0;
        priv->work_rd_buff.buffer_start = NULL;
        priv->work_rd_buff.buffer_len   = 0;
        priv->result_rd_buff            = NULL;
        priv->is_rd_closed              = 1;
    }
}


//See if there is something to read
static camio_error_t delim_read_peek( camio_stream_t* this)
{
    //This is not a public function, can assume that preconditions have been checked.
    DBG("Doing read peek\n");
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    //If this pointer is non-null, there there is already a result waiting to be consumed
    if(priv->result_rd_buff){
        return CAMIO_ENOERROR;
    }

    //First, try the easy approach, if there is data in the working buffer, try the delimiter function to see...
    if(priv->work_rd_buff.data_len > 0){
        const ch_word delimit_size = priv->delim_fn(priv->work_rd_buff.data_start, priv->work_rd_buff.data_len);
        if(delimit_size > 0 && delimit_size <= priv->work_rd_buff.data_len){
            priv->result_rd_buff = priv->work_rd_buff;
            return CAMIO_ENOERROR;
        }
    }

    //Damn, OK. That did't work, ok. Try to get some more data.
    if(!priv->base_rd_registered){
        camio_error_t err = camio_read_request(priv->base,priv->base_rd_buff_offset, priv->base_rd_src_offset);
        if(err != CAMIO_ENOERROR){
            DBG("Read request from base stream failed with error =%lli\n", err);
            return err;
        }
        //We have now registered a read request, only do this once otherwise the base stream might get confused.
        priv->base_rd_registered = true;
    }

    //Now that a read has been registered, see if there is any data available
    camio_error_t err = camio_read_ready(priv->base);
    if( err != CAMIO_EREADY){
        return err; //Damn, no data available. Come back later and try again.
    }

    //Yes! We have some more data. Get at it.
    err = camio_read_acquire(priv->base,&priv->base_rd_buff);
    if(err){
        DBG("Could not do read acquire on base stream\n");
        return err;
    }
    DBG("Read another %llibytes from %p\n", priv->base_rd_buff->data_len, priv->base_rd_buff);

    //Got some data, but is it just a closed stream?
    if(priv->base_rd_buff->data_len == 0){
        //There is no more data to read, time to give up
        delim_read_close(this);
        return CAMIO_ENOERROR;
    }

    //We have real data, try a fast path. The data read exactly matches the data that we're looking for.
    const ch_word delimit_size = priv->delim_fn(priv->base_rd_buff.data_start, priv->base_rd_buff.data_len);
    if(delimit_size == priv->base_rd_buff->data_len){
        priv->result_rd_buff = priv->base_rd_buff;
        return CAMIO_ENOERROR;
    }

    //OK. Let's put the data somewhere
    while(priv->base_rd_buff->data_len > priv->work_rd_buff.buffer_len - priv->work_rd_buff.data_len){
        DBG("Growing working buffer from %lu to %lu\n", priv->work_rd_buff.buffer_len, priv->work_rd_buff.buffer_len * 2 );
        priv->work_rd_buff.buffer_len *= 2;
        priv->work_rd_buff.buffer_start = realloc(priv->work_rd_buff.buffer_start, priv->work_rd_buff.buffer_len);
    }

    //TODO XXX, can potentially avoid this if the delimiter says that data in read_buffer includes a complete packet but have
    //to deal with a partial fragment(s) left over in the buffer.
    void* new_data_dst = (char*)priv->work_rd_buff.data_start + priv->work_rd_buff.data_len;
    DBG("Adding %lu bytes at %p (offset=%lu)\n", priv->work_rd_buff.buffer_len, new_data_dst, priv->work_rd_buff.data_len);
    memcpy(new_data_dst, priv->base_rd_buff->data_start, priv->base_rd_buff->data_len);
    priv->work_rd_buff.data_len += priv->base_rd_buff->data_len;

    //Now that the data is copied, we don't need the buffer any more.
    camio_read_release(priv->base,priv->work_rd_buff);

    //OK. Now that we've got some more data. Try one last time to delimit it.
    const ch_word delimit_size = priv->delim_fn(priv->work_rd_buff.data_start, priv->work_rd_buff.data_len);
    if(delimit_size > 0 && delimit_size <= priv->work_rd_buff.data_len){
        priv->result_rd_buff = priv->work_rd_buff;
        return CAMIO_ENOERROR;
    }

    return CAMIO_ENOERROR;
}


static camio_error_t delim_read_ready(camio_muxable_t* this)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( this->mode != CAMIO_MUX_MODE_READ){
        DBG("Wrong kind of muxable!\n"); //WTF??
        return CAMIO_EINVALID;
    }

    //OK now the fun begins
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);
    if(priv->is_rd_closed){
        return CAMIO_EREADY;
    }

    if(!priv->read_registered){ //Even if there is data waiting, nobody want's it, so ignore for now.
        DBG("Nobody wants the data\n");
        return CAMIO_ENOTREADY;
    }

    if(priv->result_buffer_size){
        return priv->result_buffer_size;
    }

    //Nope, OK, see if we can get some
    camio_error_t err = delim_read_peek(this->parent.stream);
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

static camio_error_t delim_read_request( camio_stream_t* this, ch_word buffer_offset, ch_word source_offset)
{
    (void)buffer_offset;

    DBG("Doing DELIM read request...!\n");
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( source_offset != 0){
        DBG("This is DELIM, you're not allowed to have a source offset!\n"); //WTF?
        return CAMIO_EINVALID;
    }

//    if( buffer_offset > ....){
//        DBG("You're trying to set a buffer offset (%i) which is greater than the buffer size (%i) ???\n",
//                buffer_offset, ... ); //WTF?
//        return CAMIO_EINVALID;
//    }

//    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
//    if(priv->read_registered){
//        DBG("New read request registered, but request is still outstanding\n");
//        return CAMIO_ETOOMANY;
//    }


    DBG("Doing DELIM read request...Done!..Successful\n");
    return CAMIO_ENOERROR;

}


static camio_error_t delim_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
{

    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
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
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if(priv->is_rd_closed){
        //DO STUFF HERE!!! TODO XXX
    }

    camio_error_t err = delim_read_ready(&this->rd_muxable);
//    if(err == CAMIO_EREADY){ //Whoo hoo! There's data!
//        ...
//        DBG("Got new DELIM data of size %i\n", priv->rd_buffer->data_len);
//        return CAMIO_ENOERROR;
//    }

//    if(err == CAMIO_ENOTREADY){
//        return CAMIO_ETRYAGAIN;
//    }

    //Ugh don't know what went wrong.
    DBG("Something bad happened!\n");
    return err;
}


static camio_error_t delim_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    if( NULL == buffer){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    *buffer               = NULL; //Remove dangling pointers!
    //priv->...       = ...;

    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS
 **************************************************************************************************************************/

static camio_error_t delim_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    if( NULL == buffer_o){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }
//
//    if( NULL != *buffer_o){
//        DBG("Buffer chain not null\n"); //WTF?
//        return CAMIO_EINVALID;
//    }

//    DBG("Doing write acquire\n");
//    ...

    return CAMIO_ENOERROR;
}


static camio_error_t delim_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain )
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    camio_rd_buffer_t* chain_ptr = *buffer_chain;
    while(chain_ptr != NULL){
////        DBG("Writing %li bytes from %p to %i\n", chain_ptr->data_len,chain_ptr->data_start,  ... );
        ch_word bytes = 0; //write(priv->wr_fd,chain_ptr->data_start,chain_ptr->data_len);
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

        chain_ptr = chain_ptr->__internal.__next;
    }

    return CAMIO_ENOERROR;
}


static camio_error_t delim_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer_chain)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

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
        DBG("Removing data buffer %p staring at %p\n", chain_ptr, chain_ptr->buffer_start);
//        camio_error_t err = buffer_malloc_linear_release(priv->wr_buff_pool,&chain_ptr);
//        if(err){
//            DBG("Could not release buffer??\n");
//            return err;
//        }

        //Step to the next buffer, and unlink the last
        chain_ptr_prev               = chain_ptr;
        chain_ptr                    = (*buffer_chain)->__internal.__next;
        chain_ptr_prev->__internal.__next = NULL;
    }

    *buffer_chain = NULL; //Remove dangling pointers!

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * SETUP/CLEANUP FUNCTIONS
 **************************************************************************************************************************/

static void delim_destroy(camio_stream_t* this)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return;
    }
    delim_read_close(this);

    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    (void)priv;
    //...

    free(this);

}

#define DELIM_BUFFER_DEFAULT_SIZE (64 * 1024) //64KB
camio_error_t delim_stream_construct(
    camio_stream_t* this,
    camio_connector_t* connector,
    camio_stream_t* base_stream,
    int (*delim_fn)(char* buffer, int len)
)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    priv->delim_fn  = delim_fn;
    priv->base      = base_stream;

    this->rd_muxable.mode              = CAMIO_MUX_MODE_READ;
    this->rd_muxable.parent.stream     = this;
    this->rd_muxable.vtable.ready      = delim_read_ready;
    this->rd_muxable.fd                = base_stream->rd_muxable.fd;

    this->wr_muxable.mode              = CAMIO_MUX_MODE_WRITE;
    this->wr_muxable.parent.stream     = this;
    this->wr_muxable.vtable.ready      = NULL;
    this->wr_muxable.fd                = base_stream->wr_muxable.fd;

    priv->work_rd_buff.buffer_start    = calloc(1,DELIM_BUFFER_DEFAULT_SIZE);
    priv->work_rd_buff.buffer_len      = DELIM_BUFFER_DEFAULT_SIZE;
    if(!priv->work_rd_buff.buffer_start){
        DBG("Could not allocate memory for backing buffer\n");
        return CAMIO_ENOMEM;
    }

    DBG("Done constructing DELIM stream\n");
    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(delim,delim_stream_priv_t)
