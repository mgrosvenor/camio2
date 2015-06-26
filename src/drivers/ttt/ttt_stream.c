/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   17 Nov 2014
 *  File name: ttt_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include <src/transports/stream.h>
#include <src/transports/connector.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>

#include "../../camio_debug.h"

#include "ttt_stream.h"

#include <fcntl.h>
#include <errno.h>

/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
typedef struct ttt_stream_priv_s {
    camio_connector_t connector;

} ttt_stream_priv_t;




/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/

//See if there is something to read
static camio_error_t ttt_read_peek( camio_stream_t* this)
{
    (void)this;
    //This is not a public function, can assume that preconditions have been checked.
    //DBG("Doing read peek\n");
    //ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);


//    if(errno == ....){
//        return CAMIO_ETRYAGAIN;
//    }
//    else{
//        DBG("Something else went wrong, check errno value\n");
//        return CAMIO_ECHECKERRORNO;
//    }

    return CAMIO_ENOERROR;
}


static camio_error_t ttt_read_ready(camio_muxable_t* this)
{
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( this->mode != CAMIO_MUX_MODE_READ){
        DBG("Wrong kind of muxable!\n"); //WTF??
        return CAMIO_EINVALID;
    }

    //OK now the fun begins
    //ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);

//    if(!priv->read_registered){ //Even if there is data waiting, nobody want's it, so ignore for now.
//        DBG("Nobody wants the data\n");
//        return CAMIO_ENOTREADY;
//    }

//    //Check if there is data already waiting to be dispatched
//    if(priv-> ... ){
//        DBG("There is data already waiting!\n");
//        return CAMIO_EREADY;
//    }

    //Nope, OK, see if we can get some
    camio_error_t err = ttt_read_peek(this->parent.stream);
    if(err == CAMIO_ENOERROR ){
        DBG("There is new data waiting!\n");
        return CAMIO_EREADY;
    }

//    if(err == CAMIO_ETRYAGAIN){
//        //DBG("This is no new data waiting!\n");
//        return CAMIO_ENOTREADY;
//    }

//    DBG("Something bad happened!\n");
    return err;


}

static camio_error_t ttt_read_request( camio_stream_t* this, ch_word buffer_offset, ch_word source_offset)
{
    (void)buffer_offset;

    DBG("Doing TTT read request...!\n");
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( source_offset != 0){
        DBG("This is TTT, you're not allowed to have a source offset!\n"); //WTF?
        return CAMIO_EINVALID;
    }
//    if( buffer_offset > ....){
//        DBG("You're trying to set a buffer offset (%i) which is greater than the buffer size (%i) ???\n",
//                buffer_offset, ... ); //WTF?
//        return CAMIO_EINVALID;
//    }

//    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
//    if(priv->read_registered){
//        DBG("New read request registered, but request is still outstanding\n");
//        return CAMIO_ETOOMANY;
//    }


    DBG("Doing TTT read request...Done!..Successful\n");
    return CAMIO_ENOERROR;

}


static camio_error_t ttt_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
{

    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
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
    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;

    camio_error_t err = ttt_read_ready(&this->rd_muxable);
//    if(err == CAMIO_EREADY){ //Whoo hoo! There's data!
//        ...
//        DBG("Got new TTT data of size %i\n", priv->rd_buffer->data_len);
//        return CAMIO_ENOERROR;
//    }

//    if(err == CAMIO_ENOTREADY){
//        return CAMIO_ETRYAGAIN;
//    }

    //Ugh don't know what went wrong.
    DBG("Something bad happened!\n");
    return err;
}


static camio_error_t ttt_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
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

static camio_error_t ttt_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
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


static camio_error_t ttt_write_commit(camio_stream_t* this, camio_wr_buffer_t** buffer_chain )
{
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
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


static camio_error_t ttt_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer_chain)
{
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
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

static void ttt_destroy(camio_stream_t* this)
{
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return;
    }
    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    (void)priv;
    //...

    free(this);

}

camio_error_t ttt_stream_construct(camio_stream_t* this, camio_connector_t* connector /** , ... , **/)
{
    //Basic sanity checks -- TODO TTT: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    ttt_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    priv->connector = *connector; //Keep a copy of the connector state

    this->rd_muxable.mode              = CAMIO_MUX_MODE_READ;
    this->rd_muxable.parent.stream     = this;
    this->rd_muxable.vtable.ready      = ttt_read_ready;
    //this->rd_muxable.fd                = ...

    this->wr_muxable.mode              = CAMIO_MUX_MODE_WRITE;
    this->wr_muxable.parent.stream     = this;
    this->wr_muxable.vtable.ready      = NULL;
    //this->wr_muxable.fd                = ...


    DBG("Done constructing TTT stream\n");
    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(ttt,ttt_stream_priv_t)
