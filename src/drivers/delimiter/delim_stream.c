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

    //The base variables are used to gather data from the underlying stream
    camio_buffer_t* rd_base_buff;
    camio_read_req_t* rd_base_req_vec;
    ch_word rd_base_req_vec_len;
    ch_bool rd_base_registered;

    ch_bool is_rd_closed;
    ch_bool read_registered;

    ch_word (*delim_fn)(char* buffer, ch_word len);

} delim_stream_priv_t;



/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/
static void delim_read_close(camio_stream_t* this){
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if(!priv->is_rd_closed){
        free(priv->rd_working_buff.__internal.__mem_start);
        reset_buffer(&priv->rd_working_buff);
        reset_buffer(&priv->rd_result_buff);
        priv->is_rd_closed = true;
    }
}

static camio_error_t grow_working_buff(delim_stream_priv_t* priv) {
    //     ( The amount of free space in the buffer)                                     <  (Extra space needed         )
    while ((priv->rd_working_buff.__internal.__mem_len - priv->rd_working_buff.data_len) < (priv->rd_base_buff->data_len)) {
        DBG("Growing working buffer from %lu to %lu\n", priv->rd_working_buff.__internal.__mem_len,
                priv->rd_working_buff.__internal.__mem_len * 2);
        priv->rd_working_buff.__internal.__mem_len *= 2;
        priv->rd_working_buff.__internal.__mem_start = realloc(priv->rd_working_buff.__internal.__mem_start,
                priv->rd_working_buff.__internal.__mem_len);

        if(priv->rd_working_buff.__internal.__mem_start == NULL){
            DBG("Ran out of memmory trying to grow working buff!\n");
            return CAMIO_ENOMEM;
        }
        priv->rd_working_buff.data_start = priv->rd_working_buff.__internal.__mem_start;
    }

    return CAMIO_ENOERROR;
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
            buffer_slice(&priv->rd_result_buff, &priv->rd_working_buff,priv->rd_working_buff.data_start, delimit_size );
            DBG("Yay! Delimiter first time success with data of size %lli!\n", delimit_size);
            return CAMIO_ENOERROR;
        }
    }

    //Damn, OK. That did't work, ok. Try to get some more data.
    if(!priv->rd_base_registered){
        //TODO XXX BUG: Here's a problem.. We may have to do multiple read requests here to get enough data to delimit, but,
        //the user really needed to refresh the values of priv->rd_base_buff_offset & priv->rd_base_src_offset.
        camio_error_t err = camio_read_request( priv->base, priv->rd_base_req_vec, priv->rd_base_req_vec_len );
        if(err != CAMIO_ENOERROR){
            DBG("Read request from base stream failed with error =%lli\n", err);
            return err;
        }
        //We have now registered a read request, only do this once otherwise the base stream might get confused.
        DBG("Successfully requested new data from base!\n");
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
    DBG("Data is ready to be read from base!\n");

    //Yes! We have some more data. Get at it.
    err = camio_read_acquire(priv->base,&priv->rd_base_buff);
    if(err){
        DBG("Could not do read acquire on base stream\n");
        return err;
    }
    DBG("Read another %lli bytes from %p. Data starts at %p\n", priv->rd_base_buff->data_len, priv->rd_base_buff, priv->rd_base_buff->data_start);
    priv->rd_base_registered = false; //We got the data, next time we will need to register again.

    //Got some data, but is it just a closed stream?
    if(priv->rd_base_buff->data_len == 0){
        //There is no more data to read, time to give up
        delim_read_close(this);
        DBG("Base stream is closed, closing delimiter!\n");
        return CAMIO_ENOERROR;
    }

    // ---- Now the real work begins. We have new data. So can we delimit it?

    //We have real data and the working buffer is empty, try a fast path. Maybe we won't need to copy into the working buffer
    if(priv->rd_working_buff.data_len == 0){
        DBG("Trying empty working buffer fast path...\n");
        const ch_word delimit_size = priv->delim_fn(priv->rd_base_buff->data_start, priv->rd_base_buff->data_len);
        if(delimit_size > 0) { //Success! There is a valid packet in the read buffer alone.
            buffer_slice(&priv->rd_result_buff,priv->rd_base_buff,priv->rd_base_buff->data_start, delimit_size);
            DBG("Fast path match from delimiter of size %lli!\n", delimit_size);
            return CAMIO_ENOERROR;
        }
        DBG("Datagram fast path fail. Try slow path...\n");
    }

    //Just check that this is assumption true. If it's not, the following code will break!
    if(priv->rd_working_buff.data_start != priv->rd_working_buff.__internal.__mem_start){
        DBG("Invariant violation. Assumptions will be broken!\n");
        return CAMIO_EINVALID;
    }

    //Let's put the data somewhere that we can work with it later. First make sure we have enough space:
    err = grow_working_buff(priv);
    if (err){
        DBG("Could not grow working buff\n");
        return err;
    }

    //Now copy the data over into the working buffer.
    void* new_data_dst = (char*)priv->rd_working_buff.data_start + priv->rd_working_buff.data_len;
    DBG("Adding %lu bytes at %p (offset=%lu)\n", priv->rd_base_buff->data_len, new_data_dst, priv->rd_working_buff.data_len);
    memcpy(new_data_dst, priv->rd_base_buff->data_start, priv->rd_base_buff->data_len);
    priv->rd_working_buff.data_len += priv->rd_base_buff->data_len;

    //Now that the data is copied, we don't need the read buffer any more.
    camio_read_release(priv->base,&priv->rd_base_buff);

    //OK. Now that we've got some more data. Try one last time to delimit it again.
    const ch_word delimit_size = priv->delim_fn(priv->rd_working_buff.data_start, priv->rd_working_buff.data_len);
    if(delimit_size > 0 && delimit_size <= priv->rd_working_buff.data_len){
        buffer_slice(&priv->rd_result_buff,&priv->rd_working_buff,priv->rd_working_buff.data_start, delimit_size);
        DBG("Slow path  match from delimiter of size %lli!\n", delimit_size);
        return CAMIO_ENOERROR;
    }

    return CAMIO_ETRYAGAIN; //OK. Well we tried. Come back here and try again another time.
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
        DBG("This is no new data waiting!\n");
        return CAMIO_ENOTREADY;
    }

    DBG("Something bad happened err=%lli!\n", err);
    return err;


}

static camio_error_t delim_read_request(camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len)
{
    DBG("Doing delim read request...!\n");
    //Basic sanity checks -- TODO  Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if(req_vec_len != 1){
        DBG("request length of %lli requested. This value is not currently supported\n", req_vec_len);
        return CAMIO_NOTIMPLEMENTED;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->read_registered){
        DBG("New read request registered, but old request is still outstanding. Ignoring request.\n");
        return CAMIO_ETOOMANY;
    }

    //Hang on to these for another time when it is needed TODO XXX see the delimiter function for a bug related to this...
    if(req_vec->dst_offset_hint != 0){
        DBG("Delimiter currently only supports buffer offsets of 0\n");
        return CAMIO_EINVALID; //TODO XXX: Better error
    }
    priv->rd_base_req_vec = req_vec;
    priv->rd_base_req_vec_len = req_vec_len;

    //OK. Register the read
    priv->read_registered = true;

    DBG("Doing delim read request...Done!..Successful\n");
    return CAMIO_ENOERROR;

}


static camio_error_t delim_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
{
    DBG("Acquiring buffer\n");

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
        DBG("Delimiter is closed, returning null\n");
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

    if(!priv->read_registered){
        DBG("WFT? Why are you trying to release something if you haven't read anyhting?\n");
        return CAMIO_EINVALID;
    }

    //Deal with the basic release operations.
    priv->read_registered = false;
    *buffer = NULL; //Remove dangling pointer for the source

     //We arrived here because for one of two reasons
    //  1) we read successfully delimited from the base stream. So we're still hanging on to the base stream buffer
    //  2) we successfully delimited from the working buffer.
    //  Regardless of how we got here, we have to decide if we're going to move data to the front of the buffer, or
    //  if there is another packet waiting. So first, we figure out if the is any data left to delimit

    //Step 1 - figure out how much data is waiting to be consumed
    void* data_next           = (char*)priv->rd_result_buff.data_start + priv->rd_result_buff.data_len ;
    const ch_word data_offset = (char*)data_next - (char*)priv->rd_result_buff.__internal.__mem_start;
    const void* mem_end       = (char*)priv->rd_result_buff.__internal.__mem_start + priv->rd_result_buff.__internal.__mem_len;
    DBG("Data offset = %lli\n", data_offset);
    const ch_word data_remain = priv->rd_result_buff.__internal.__mem_len - data_offset;
    DBG("There is %lli bytes remaining in the result buffer\n", data_remain);

    //If there is nothing left over, give up and bug out
    if(data_next >= mem_end){
        DBG("There's no data left. We're done!\n");
        if(priv->rd_base_buff){
            DBG("Releasing underlying fast_path buffer\n");
            //The result is directly from the underlying stream, so we should release it now.
            camio_read_release(priv->base,&priv->rd_base_buff);
        }
        goto reset_and_exit;
    }

    //OK. so there's some data hanging around, let's try to delimit it

    const ch_word data_next_len = data_remain;

    const ch_word delimit_size = priv->delim_fn(data_next, data_next_len);
    if(delimit_size > 0 && delimit_size <= data_next_len){
        //Ready for the next round with data available! No memmove required!
        DBG("Success! No memmove required this time!\n");
        priv->rd_result_buff.data_start = data_next;
        priv->rd_result_buff.data_len   = delimit_size;
        return CAMIO_ENOERROR;
    }

    printf("Data offset = %lli, mem_len=%lli, detla=%lli\n",
            data_offset,
            priv->rd_result_buff.__internal.__mem_len,
            priv->rd_result_buff.__internal.__mem_len - data_offset
    );
    printf("There is %lli bytes remaining in the result buffer\n", data_remain);
    //Nope. No success with the delimiter. Looks like we have to move stuff arround. Move waht we have right back to the
    //beginning of the working buffer. First make sure that the buffer is big enough
    camio_error_t err = grow_working_buff(priv);
    if (err){
        DBG("Could not grow working buff\n");
        return err;
    }

    printf("Doing mem move of %lli from %p to %p\n", data_next_len, data_next,
            priv->rd_working_buff.__internal.__mem_start );
    memmove(priv->rd_working_buff.__internal.__mem_start, data_next, data_next_len);
    priv->rd_working_buff.data_start = priv->rd_working_buff.__internal.__mem_start;
    priv->rd_working_buff.data_len   = data_next_len;

    //If we still have a base buffer at this point, we should get rid of it
    if(priv->rd_base_buff){
        DBG("Releasing underlying fast_path buffer\n");
        //The result is directly from the underlying stream, so we should release it now.
        camio_read_release(priv->base,&priv->rd_base_buff);
    }

reset_and_exit:
    DBG("Releasing result buffer and exiting\n");
    reset_buffer(&priv->rd_result_buff); //We're done with this buffer pointer for the moment until there's new data

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


static camio_error_t delim_write_request(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    delim_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    return camio_write_request(priv->base,req_vec,req_vec_len);
}


static camio_error_t delim_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer_chain)
{
    DBG("Doing delimiter write release\n");
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
    if(priv->rd_working_buff.__internal.__mem_start){
        free(priv->rd_working_buff.__internal.__mem_start);
    }

    camio_stream_destroy(priv->base);

    free(this);

}

#define DELIM_BUFFER_DEFAULT_SIZE (64 * 1024) //64KB
camio_error_t delim_stream_construct(
    camio_stream_t* this,
    camio_connector_t* connector,
    camio_stream_t* base_stream,
    ch_word(*delim_fn)(char* buffer, ch_word len)
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

    priv->rd_working_buff.__internal.__mem_start = calloc(1,DELIM_BUFFER_DEFAULT_SIZE);
    priv->rd_working_buff.__internal.__mem_len   = DELIM_BUFFER_DEFAULT_SIZE;
    priv->rd_working_buff.data_start   = priv->rd_working_buff.__internal.__mem_start;
    if(!priv->rd_working_buff.__internal.__mem_start){
        DBG("Could not allocate memory for backing buffer\n");
        return CAMIO_ENOMEM;
    }

    DBG("Done constructing DELIM stream\n");
    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(delim,delim_stream_priv_t)
