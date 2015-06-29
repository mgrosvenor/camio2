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
#include <src/api/api.h>

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
    camio_buffer_t rd_working_buff;

    //The result buffer is where we put stuff when there is a result to send out to the outside world
    camio_buffer_t rd_result_buff;

    //The base buffer is used to gather data from the base stream
    camio_buffer_t* rd_base_buff;
    ch_word rd_base_buff_offset;     //Where should data be placed in the read buffer
    ch_word rd_base_src_offset;   //Where should data be placed in the read buffer
    ch_bool rd_base_registered;

    ch_bool is_rd_closed;

    ch_bool read_registered;

    int (*delim_fn)(char* buffer, int len);

} delim_stream_priv_t;


//Reset the buffers internal pointers to point to nothing
static inline void reset_buffer(camio_buffer_t* dst)
{
    dst->buffer_start       = NULL;
    dst->buffer_len         = 0;
    dst->data_start         = NULL;
    dst->data_len           = 0;
}


//Assign the pointers from one buffer to another
static inline void assign_buffer(camio_buffer_t* dst, camio_buffer_t* src, void* data_start, ch_word data_len)
{
    dst->buffer_start       = src->buffer_start;
    dst->buffer_len         = src->buffer_len;
    dst->data_start         = data_start;
    dst->data_len           = data_len;
}


/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/
static void delim_read_close(camio_stream_t* this){
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if(!priv->is_rd_closed){
        free(priv->rd_working_buff.buffer_start);
        reset_buffer(&priv->rd_working_buff);
        reset_buffer(&priv->rd_result_buff);
    }
}




//See if there is something to read
static camio_error_t delim_read_peek( camio_stream_t* this)
{
    //This is not a public function, can assume that preconditions have been checked.
//    DBG("Doing read peek\n");
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    //If this pointer is non-null, there there is already a result waiting to be consumed
    if(priv->rd_result_buff.data_start){
        DBG("Already have a result buffer\n");
        return CAMIO_ENOERROR;
    }

    //First, try the easy approach, if there is data in the working buffer, try the delimiter function to see...
    if(priv->rd_working_buff.data_len > 0){
        const ch_word delimit_size = priv->delim_fn(priv->rd_working_buff.data_start, priv->rd_working_buff.data_len);
        if(delimit_size > 0 && delimit_size <= priv->rd_working_buff.data_len){
            assign_buffer(&priv->rd_result_buff, &priv->rd_working_buff,priv->rd_working_buff.data_start, delimit_size );
            DBG("Yay! Delimiter first time success with data of size %lli!\n", delimit_size);
            return CAMIO_ENOERROR;
        }
    }

    //Damn, OK. That did't work, ok. Try to get some more data.
    if(!priv->rd_base_registered){
        //TODO XXX BUG: Here's a problem.. We may have to do multiple read requests here to get enough data to delimit, but,
        //the user really needed to refresh the values of priv->rd_base_buff_offset & priv->rd_base_src_offset.
        camio_error_t err = camio_read_request(priv->base,priv->rd_base_buff_offset, priv->rd_base_src_offset);
        if(err != CAMIO_ENOERROR){
            DBG("Read request from base stream failed with error =%lli\n", err);
            return err;
        }
        //We have now registered a read request, only do this once otherwise the base stream might get confused.
        DBG("Requested new data from base!\n");
        priv->rd_base_registered = true;
    }

    //Now that a read has been registered, see if there is any data available
    camio_error_t err = camio_read_ready(priv->base);
    if(err == CAMIO_ENOTREADY){
        return CAMIO_ETRYAGAIN;
    }
    if( err != CAMIO_EREADY){
        DBG("Unexpected error from base ready()!\n");
        return err; //Damn, no data available. Come back later and try again.
    }

    //Yes! We have some more data. Get at it.
    err = camio_read_acquire(priv->base,&priv->rd_base_buff);
    if(err){
        DBG("Could not do read acquire on base stream\n");
        return err;
    }
    DBG("Read another %lli bytes from %p\n", priv->rd_base_buff->data_len, priv->rd_base_buff);
    priv->rd_base_registered = false; //We got the data, next time we will need to register again.

    //Got some data, but is it just a closed stream?
    if(priv->rd_base_buff->data_len == 0){
        //There is no more data to read, time to give up
        delim_read_close(this);
        DBG("Base stream is closed, closing delimiter!\n");
        return CAMIO_ENOERROR;
    }

    //We have real data and the working buffer is empty, try a fast path. If the data read exactly matches the data that
    //we're looking for then there is no need to copy it into the working buffer.
    if(priv->rd_working_buff.data_len == 0){
        DBG("Trying empty buffer fast path...\n");
        const ch_word delimit_size = priv->delim_fn(priv->rd_base_buff->data_start, priv->rd_base_buff->data_len);
        if(delimit_size == priv->rd_base_buff->data_len){
            assign_buffer(&priv->rd_result_buff,priv->rd_base_buff,priv->rd_base_buff->data_start, delimit_size);
            DBG("Fast path exact match from delimiter of size %lli!\n", delimit_size);
            return CAMIO_ENOERROR;
        }
        DBG("Fast path fail. Try slow path...\n");
    }

    //Just check that this is true. If it's not, the following code's assumptions will be violated.
    if(priv->rd_working_buff.data_start != priv->rd_working_buff.buffer_start){
        DBG("Invariant violation. Assumptions will be broken!\n");
        return CAMIO_EINVALID;
    }

    //OK. Let's put the data somewhere that we can work with it later. First make sure we have enough space
    //     ( The amount of free space in the buffer)                          <  (Extra space needed         )
    while( (priv->rd_working_buff.buffer_len - priv->rd_working_buff.data_len) < (priv->rd_base_buff->data_len)  ){
        DBG("Growing working buffer from %lu to %lu\n", priv->rd_working_buff.buffer_len, priv->rd_working_buff.buffer_len * 2 );
        priv->rd_working_buff.buffer_len *= 2;
        priv->rd_working_buff.buffer_start = realloc(priv->rd_working_buff.buffer_start, priv->rd_working_buff.buffer_len);
        priv->rd_working_buff.data_start   = priv->rd_working_buff.buffer_start;
    }

    //TODO XXX, can potentially avoid this if the delimiter says that data in read_buffer includes a complete packet but have
    //to deal with a partial fragment(s) left over in the buffer.
    void* new_data_dst = (char*)priv->rd_working_buff.buffer_start + priv->rd_working_buff.data_len;
    DBG("Adding %lu bytes at %p (offset=%lu)\n", priv->rd_base_buff->data_len, new_data_dst, priv->rd_working_buff.data_len);
    memcpy(new_data_dst, priv->rd_base_buff->data_start, priv->rd_base_buff->data_len);
    priv->rd_working_buff.data_len += priv->rd_base_buff->data_len;

    //Now that the data is copied, we don't need the buffer any more.
    camio_read_release(priv->base,&priv->rd_base_buff);

    //OK. Now that we've got some more data. Try one last time to delimit it.
    const ch_word delimit_size = priv->delim_fn(priv->rd_working_buff.data_start, priv->rd_working_buff.data_len);
    if(delimit_size > 0 && delimit_size <= priv->rd_working_buff.data_len){
        assign_buffer(&priv->rd_result_buff,&priv->rd_working_buff,priv->rd_working_buff.data_start, delimit_size);
        DBG("Slow path  match from delimiter of size %lli!\n", delimit_size);
        return CAMIO_ENOERROR;
    }

    return CAMIO_ETRYAGAIN; //We've got nothing
}


static camio_error_t delim_read_ready(camio_muxable_t* this)
{
    DBG("Checking if delimiter is ready\n");

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
        DBG("Stream is now closed. There will be no more non null data\n");
        return CAMIO_EREADY;
    }

    if(!priv->read_registered){ //Even if there is data waiting, nobody want's it, so ignore for now.
        DBG("Nobody wants the data\n");
        return CAMIO_ENOTREADY;
    }

    if(priv->rd_result_buff.data_start){
        return CAMIO_EREADY;
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

    DBG("Something bad happened err=%lli!\n", err);
    return err;


}

static camio_error_t delim_read_request( camio_stream_t* this, ch_word buffer_offset, ch_word source_offset)
{
    DBG("Doing delim read request...!\n");
    //Basic sanity checks -- TODO  Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->read_registered){
        DBG("New read request registered, but old request is still outstanding. Ignoring request.\n");
        return CAMIO_ETOOMANY;
    }

    //Hang on to these for another time when it is needed TODO XXX see the dilimter function for a bug related to this...
    if(buffer_offset != 0){
        DBG("Delimiter only supports buffer offsets of 0\n");
        return CAMIO_EINVALID; //TODO XXX: Better error
    }
    priv->rd_base_buff_offset = buffer_offset;
    priv->rd_base_src_offset  = source_offset;

    //OK. Register the read
    priv->read_registered = true;

    DBG("Doing delim read request...Done!..Successful\n");
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
        reset_buffer(&priv->rd_result_buff);
        *buffer_o = &priv->rd_result_buff;
        return CAMIO_ENOERROR;
    }

    if(!priv->read_registered){
        DBG("Requested a buffer, but not read registered. Ignoring\n");
        return CAMIO_EINVALID;
    }

    if(priv->rd_result_buff.data_start){ //yay! There's data!
        *buffer_o = &priv->rd_result_buff;
        DBG("Returning data that's been waiting\n");
        return CAMIO_ENOERROR;
    }

    //Read is registered, but there's no data yet, try seeing if there is some data ready.
    camio_error_t err = delim_read_ready(&this->rd_muxable);
    if(err == CAMIO_EREADY){ //Whoo hoo! There's data!
        *buffer_o = &priv->rd_result_buff;
        DBG("Got new DELIM data of size %i\n", priv->rd_result_buff.data_len);
        return CAMIO_ENOERROR;
    }

    if(err == CAMIO_ENOTREADY){
        return CAMIO_ETRYAGAIN;
    }

    //Ugh don't know what went wrong.
    DBG("Something bad happened!\n");
    return err;
}


/**
 * Warning: This is a complicated function with a lot of edge cases. It's vital to performance and correctness to get this
 *          one right!
 *
 *  We enter this function as a result of a successful call to read_acquire, which implies a successful call to
 *  read_peek(). read_peek() may have succeeded for one of two reasons,
 *      (1) either new data was read and the delimiter was successful. This means that priv->work_rd_buff.data_len is at
 *          least equal to priv->result_rd_buff.data_len. This has two sub-cases:
 *          (a) The delimiter was successful because the working buffer was empty and the base stream returned a result that
 *              is the exact size the delimiter was expecting. In this case, the delimiter is basically "pass-through"
 *              and the base stream then needs read_release to be called.
 *          (b) The delimiter was successful because it found a match in the working buffer. In this case, we need to resize
 *              and potentially move data.
 *      (2) We have optimistically found another delimited result in a previous call to read_release().  In this case
 *          priv->work_rd_buff.data_len may be less than priv->result_rd_buff.data_size. Again, we may need to resize and
 *          move data.
 *
 *  We must handle both of these cases in this function
 */
static camio_error_t delim_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    DBG("Releasing read\n");

    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);


    if( NULL == buffer){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    //Calculate this now, we'll need it later
    char* result_head_next = (char*)priv->rd_result_buff.data_start + priv->rd_result_buff.data_len ;

    //Handle case 1a)
    if(priv->rd_result_buff.data_start == priv->rd_base_buff->data_start){
        //The result is directly from the underlying stream, so we should release it now.
        camio_read_release(priv->base,&priv->rd_base_buff);
        goto reset_and_exit;
    }

    if(priv->rd_working_buff.data_len >= priv->rd_result_buff.data_len ){

        //This is how much is now left over
        priv->rd_working_buff.data_len -= priv->rd_result_buff.data_len;

        //If there is nothing left over, give up and bug out
        if(!priv->rd_working_buff.data_len){
            goto reset_and_exit;
        }

        //Something is left over we can optimistically check if there happens to be another packet ready to go.
        //The reduces the number of memmoves and significantly improves performance especially with TCP where the
        //read size can be *huge*. (>4MB with TSO turned on)
        const ch_word delimit_size = priv->delim_fn(result_head_next, priv->rd_working_buff.data_len);
        if(delimit_size > 0 && delimit_size <= priv->rd_working_buff.data_len){
            //Ready for the next round with data available! No memmove required!
            assign_buffer(&priv->rd_result_buff,&priv->rd_working_buff,result_head_next,delimit_size);
            return CAMIO_ENOERROR;
        }

        //Nope, ok, bite the bullet and move stuff around. We'll need to call read() and get some more data at a later stage
        DBG("case 1b) - doing mem move of %lu from %p to %p (total=%lu)\n",
            priv->rd_working_buff.data_len,
            result_head_next,
            priv->rd_working_buff.data_start,
            (char*)result_head_next - (char*)priv->rd_working_buff.data_start
        );
        memmove(priv->rd_working_buff.data_start, result_head_next, priv->rd_working_buff.data_len);
        goto reset_and_exit; //goto is not strictly necessary, but this is what the program flow will do
    }

    //Handle case 2)
    //There is some data leftover, but less than the last result buffer size.
    else{

        //Optimistically check if there is a new packet in this left over. Remember, packets are not all the same
        //size, so a small packet could follow a big packet.
        const ch_word delimit_size = priv->delim_fn(result_head_next, priv->rd_working_buff.data_len);
        if(delimit_size > 0 && delimit_size <= priv->rd_working_buff.data_len){
            //Ready for the next round with data available! No memmove required!
            assign_buffer(&priv->rd_result_buff,&priv->rd_working_buff,result_head_next,delimit_size);
            return CAMIO_ENOERROR;
        }

        //Nope, ok, bite the bullet and move stuff around. We'll need to call read() and get some more data
        DBG("case 2) - doing mem move of %lu from %p to %p (total=%lu)\n",
            priv->rd_working_buff.data_len,
            result_head_next,
            priv->rd_working_buff.data_start,
            (char*)result_head_next - (char*)priv->rd_working_buff.data_start
        );
        memmove(priv->rd_working_buff.data_start, result_head_next, priv->rd_working_buff.data_len);
        goto reset_and_exit; //goto is not strictly necessary, but this is what the program flow will do
    }

    //Ready for the next round, there is no new delimited packet available, so reset everything and get ready for a call to
    //read more data
reset_and_exit:
    reset_buffer(&priv->rd_result_buff); //We're done with this buffer pointer for the moment until there's new data
    priv->read_registered   = false;
    *buffer                 = NULL; //Remove dangling pointer for the source

    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS
 **************************************************************************************************************************/
static camio_error_t delim_write_ready(camio_muxable_t* this)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( this->mode != CAMIO_MUX_MODE_WRITE){
        DBG("Wrong kind of muxable!\n"); //WTF??
        return CAMIO_EINVALID;
    }

    //OK now the fun begins
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);

    return camio_write_ready(priv->base);

}

static camio_error_t delim_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    return camio_write_acquire(priv->base,buffer_o);
}


static camio_error_t delim_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain )
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    return camio_write_commit(priv->base,buffer_chain);
}


static camio_error_t delim_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer_chain)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    return camio_write_release(priv->base,buffer_chain);
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
    if(priv->rd_working_buff.buffer_start){
        free(priv->rd_working_buff.buffer_start);
    }

    camio_stream_destroy(priv->base);

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
    (void)connector; //Nothing to do with this

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
    this->wr_muxable.vtable.ready      = delim_write_ready;
    this->wr_muxable.fd                = base_stream->wr_muxable.fd;

    priv->rd_working_buff.buffer_start = calloc(1,DELIM_BUFFER_DEFAULT_SIZE);
    priv->rd_working_buff.buffer_len   = DELIM_BUFFER_DEFAULT_SIZE;
    priv->rd_working_buff.data_start   = priv->rd_working_buff.buffer_start;
    if(!priv->rd_working_buff.buffer_start){
        DBG("Could not allocate memory for backing buffer\n");
        return CAMIO_ENOMEM;
    }

    DBG("Done constructing DELIM stream\n");
    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(delim,delim_stream_priv_t)
