/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_stream.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <sys/mman.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>

#include <src/transports/stream.h>
#include <src/transports/connector.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>
#include <src/api/api.h>
#include <src/camio_debug.h>

#include "bring_stream.h"
#include "bring_transport.h"

/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
#define CAMIO_BRING_BUFFER_SIZE (4 * 1024 * 1024)
//Current (simple) BRING recv only does one buffer at a time, this could change with vectored I/O in the future.
#define CAMIO_BRING_BUFFER_COUNT (1)

typedef struct bring_stream_priv_s {
    camio_controller_t connector;
    bring_params_t params;
    volatile bring_header_t* bring_head;
    ch_bool connected; //Is the stream currently running? Or has close been called?

    //The current read buffer
    volatile char* rd_mem;
    ch_bool read_registered;        //Has a read been registered?
    //ch_bool read_ready;           //Is the stream ready for writing (for edge triggered multiplexers)
    camio_buffer_t* rd_buffers;     //All buffers for the read side;
    ch_word rd_buffers_count;
    camio_buffer_t* rd_buff;        //A place to keep a read buffer until it's ready to be consumed
    camio_read_req_t* read_req;     //Read request vector to put data into when there is new data
    ch_word read_req_len;           //size of the request vector
    ch_word read_req_curr;          //This will be needed later
    ch_word rd_sync_counter;            //Synchronization counter
    ch_word rd_acq_index;           //Current index for acquiring new read buffers
    ch_word rd_rel_index;           //Current index for releasing new read buffers


    volatile char* wr_mem;
    ch_bool write_registered;     //Has a write been registered?
    ch_bool write_ready;          //Is the stream ready for writing (for edge triggered multiplexers)
    camio_buffer_t* wr_buffers;   //All buffers for the write side;
    ch_word wr_buffers_count;
    camio_write_req_t* write_req; //Write request vector to take data out of when there is new data.
    ch_word write_req_len;        //size of the request vector
    ch_word write_req_curr;
    ch_word write_ready_curr;

    size_t wr_bring_size;         //Size of the bring buffer
    ch_word wr_sync_counter;      //Synchronization counter
    ch_word wr_slot_size;         //Size of each slot in the ring
    ch_word wr_slot_count;        //Number of slots in the ring
    ch_word wr_rel_index;         //Current slot in the bring

} bring_stream_priv_t;




/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/

//See if there is something to read
static camio_error_t bring_read_peek( camio_stream_t* this)
{
    DBG("Doing read peek\n");

    //This is not a public function, can assume that preconditions have been checked.
    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->rd_buff){ //There is a ready already waiting
        return CAMIO_ENOERROR;
    }


    //Is there new data?
    volatile char* slot_mem     = (volatile void*)priv->rd_buffers[priv->rd_acq_index].__internal.__mem_start;
    const ch_word slot_mem_sz  = priv->rd_buffers[priv->rd_acq_index].__internal.__mem_len;
    volatile char* hdr_mem      = slot_mem + slot_mem_sz; // - sizeof(bring_slot_header_t);

    DBG("Doing read peek on index %lli at %p\n", priv->rd_acq_index, slot_mem, hdr_mem );

    __sync_synchronize();
    volatile bring_slot_header_t curr_slot_head = *(volatile bring_slot_header_t*)(hdr_mem);
    __sync_synchronize();

    DBG("seq no   =%lli offset=(%lli)\n", curr_slot_head.seq_no, (char*)hdr_mem - (char*)priv->bring_head);
    DBG("data size=%lli\n", curr_slot_head.data_size);

    if(curr_slot_head.data_size > slot_mem_sz){
        ERR("Data size is larger than memory size, corruption is likely!\n");
        return CAMIO_ENOERROR;
    }

    if( curr_slot_head.seq_no == 0){
        return CAMIO_ETRYAGAIN;
    }

    if( curr_slot_head.seq_no != priv->rd_sync_counter){
        DBG( "Ring overflow. This should not happen with a blocking ring %llu to %llu\n",
            curr_slot_head.seq_no,
            priv->rd_sync_counter
        );
        return CAMIO_EWRONGBUFF;
    }

    priv->rd_buff                       = &priv->rd_buffers[priv->rd_acq_index];
    priv->rd_buff->data_start           = (void*)slot_mem;
    priv->rd_buff->data_len             = curr_slot_head.data_size;
    priv->rd_buff->valid                = true;
    priv->rd_buff->__internal.__in_use  = true;
    priv->rd_sync_counter++;

    priv->rd_acq_index++;   //Move to the next index
    if(priv->rd_acq_index >= priv->rd_buffers_count){ //But loop around
        priv->rd_acq_index = 0;
    }

    DBG("acq_index=%lli buffers_count=%lli\n", priv->rd_acq_index, priv->rd_buffers_count);


    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_ready(camio_muxable_t* this)
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
    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);

    if(!priv->read_registered){ //Even if there is data waiting, nobody want's it, so ignore for now.
        DBG("Nobody wants the data\n");
        return CAMIO_ENOTREADY;
    }

    //Check if there is data already waiting to be dispatched
    if(priv->rd_buff){
        DBG("There is data already waiting!\n");
        return CAMIO_EREADY;
    }

    //Nope, OK, see if we can get some
    camio_error_t err = bring_read_peek(this->parent.stream);
    if(err == CAMIO_ENOERROR ){
        DBG("There is new data waiting!\n");
        return CAMIO_EREADY;
    }

    if(err == CAMIO_ETRYAGAIN){
        //DBG("This is no new data waiting!\n");
        return CAMIO_ENOTREADY;
    }

    DBG("Something bad happened! Error=%lli\n", err);
    return err;


}

static camio_error_t bring_read_request( camio_stream_t* this, camio_read_req_t* req_vec, ch_word req_vec_len )
{
    DBG("Doing bring read request...!\n");
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

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

    DBG("Doing bring read request...Done!..Successful\n");
    return CAMIO_ENOERROR;

}


static camio_error_t bring_read_acquire( camio_stream_t* this,  camio_rd_buffer_t** buffer_o)
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

    camio_error_t err = bring_read_ready(&this->rd_muxable);
    if(err == CAMIO_ENOTREADY){
        return CAMIO_ETRYAGAIN;
    }

    if(err != CAMIO_EREADY){    //Ugh don't know what went wrong.
        ERR("Something bad happened! Err no =%lli\n", err);
        return err;
    }

    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    *buffer_o = priv->rd_buff;
    priv->read_registered = false; //Release the read now. This means that read req can be called again
    DBG("Returning new buffer of size %lli at %p\n", (*buffer_o)->data_len, (*buffer_o)->data_start);
    priv->rd_buff         = NULL;  //Release the temp buffer pointer

    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_release(camio_stream_t* this, camio_rd_buffer_t** buffer)
{
    DBG("Doing read release\n");

    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        ERR("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == buffer){
        ERR("Buffer pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        ERR("Buffer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if((*buffer)->__internal.__parent != this){
        //TODO XXX could add this feature but it would be non-trivial
        ERR("Cannot release a buffer that does not belong to us!\n");
        return CAMIO_EINVALID;
    }

    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    camio_rd_buffer_t* bring_buffer = *buffer;

    //Check that the buffers are being freed in order
    if(bring_buffer->__internal.__buffer_id != priv->rd_rel_index){
        ERR("Cannot release this buffer (id=%lli) until buffer with id=%lli is released\n",
            bring_buffer->__internal.__buffer_id,
            priv->rd_rel_index);
        return CAMIO_EWRONGBUFF;
    }

    DBG("Trying to relase buffer at index=%lli\n", priv->rd_rel_index);
    volatile char* slot_mem                         = (volatile void*)bring_buffer->__internal.__mem_start;
    const uint64_t slot_mem_sz                      = bring_buffer->__internal.__mem_len;
    volatile char* hdr_mem                          = slot_mem + slot_mem_sz;
    volatile bring_slot_header_t* curr_slot_head    = (volatile bring_slot_header_t*)(hdr_mem);

    curr_slot_head->data_size = 0;
    //Apply an atomic update to tell the write end that we received this data
    //1 - Make sure all the memory writes are done
    __sync_synchronize();
    //2 - Do a word aligned single word write (atomic)
    (*(volatile uint64_t*)&curr_slot_head->seq_no) = 0x0ULL;
    //3 - Again, make sure all the memory writes are done
    __sync_synchronize();

    bring_buffer->__internal.__in_use   = false;
    bring_buffer->valid                 = false;
    *buffer                             = NULL; //Remove dangling pointers!

    //We're done. Increment the buffer index and wrap around if necessary -- this is faster than using a modulus (%)
    priv->rd_rel_index++;
    if(priv->rd_rel_index >= priv->rd_buffers_count){
        priv->rd_rel_index = 0;
    }

    DBG("Success!\n");

    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS
 **************************************************************************************************************************/
static camio_error_t try_write_acquire(camio_stream_t* this){
    DBG("Doing write acquire\n");

    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
      if( NULL == this){
          DBG("This null???\n"); //WTF?
          return CAMIO_EINVALID;
      }
      bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

      DBG("Trying to do a write acquire at index=%lli\n", priv->wr_rel_index);


      //Is there a new slot ready for writing?
      camio_buffer_t* result      = &priv->wr_buffers[priv->wr_rel_index];
      volatile char* slot_mem     = (volatile void*)result->__internal.__mem_start;
      const uint64_t slot_mem_sz  = result->__internal.__mem_len;
      volatile char* hdr_mem      = slot_mem + slot_mem_sz; // - sizeof(bring_slot_header_t);
      __sync_synchronize();
      register bring_slot_header_t curr_slot_head = *(volatile bring_slot_header_t*)(hdr_mem);
      __sync_synchronize();

      DBG("seq no   =%lli offset=(%lli)\n", curr_slot_head.seq_no, (char*)hdr_mem - (char*)priv->bring_head);
      DBG("data size=%lli\n", curr_slot_head.data_size);


      if( curr_slot_head.seq_no != 0x00ULL){
          return CAMIO_ETRYAGAIN;
      }

      //We're all good. A buffer is ready and waiting to to be acquired
      return CAMIO_ENOERROR;
}


//This should return a buffer when there is one available.
static camio_error_t bring_write_acquire(camio_stream_t* this, camio_wr_buffer_t** buffer_o)
{
    DBG("Doing write acquire\n");

    if( NULL == buffer_o){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL != *buffer_o){
        DBG("Buffer chain not null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    camio_error_t err = try_write_acquire(this);
    if( err != CAMIO_ENOERROR){
        DBG("Try write got an error %lli\n", err);
        return err;
    }

    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    camio_buffer_t* result      = &priv->wr_buffers[priv->wr_rel_index];
    volatile char* slot_mem     = (volatile void*)result->__internal.__mem_start;
    const uint64_t slot_mem_sz  = result->__internal.__mem_len;

    result->data_start  = (void*)slot_mem;
    result->data_len    = slot_mem_sz; // - sizeof(bring_slot_header_t);
    result->valid       = true;
    result->__internal.__in_use = true;
    *buffer_o           = result;

    DBG("Returning new buffer of size %lli at %p, index=%lli\n", (*buffer_o)->data_len, (*buffer_o)->data_start, priv->wr_rel_index);
    return CAMIO_ENOERROR;
}


//Try to write to the underlying, stream. This function is private, so no precondition checks necessary
static camio_error_t bring_write_try(camio_stream_t* this)
{
    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    for(int i = priv->write_req_curr; i < priv->write_req_len; i++, priv->write_req_curr++){
        camio_write_req_t* req = priv->write_req + i;
        camio_buffer_t* buff   = req->buffer;

        //Check that the buffers are being used in order
        if(buff->__internal.__buffer_id != priv->wr_rel_index){
            ERR("Cannot use/release this buffer (id=%lli) until buffer with id=%lli is released\n",
                buff->__internal.__buffer_id,
                priv->wr_rel_index
            );
            return CAMIO_EWRONGBUFF;
        }

        //Check that the data is not corrupted
        if(buff->data_len > buff->__internal.__mem_len){
            ERR("Data length (%lli) is longer than buffer length (%lli), corruption has probably occured\n",
                buff->data_len,
                buff->__internal.__mem_len
            );
            return CAMIO_ETOOMANY;
        }

        DBG("Trying to writing %li bytes from %p to %p (offset = %lli)\n", buff->data_len,buff->data_start, ((char*)priv->rd_buffers - (char*)buff->data_start));

        volatile char* slot_mem                         = (volatile void*)buff->__internal.__mem_start;
        const uint64_t slot_mem_sz                      = buff->__internal.__mem_len;
        volatile char* hdr_mem                          = slot_mem + slot_mem_sz; // - sizeof(bring_slot_header_t);
        volatile bring_slot_header_t* curr_slot_head = (volatile bring_slot_header_t*)(hdr_mem);

        DBG("seq no   =%lli offset=(%lli)\n", curr_slot_head->seq_no, (char*)hdr_mem - (char*)priv->bring_head);
        DBG("data size=%lli\n", curr_slot_head->data_size);

        curr_slot_head->data_size = buff->data_len;
        //Apply an atomic update to tell the write end that we received this data
        //1 - Make sure all the memory writes are done
        __sync_synchronize();
        //2 - Do a word aligned single word write (atomic)
        *(volatile uint64_t*)&curr_slot_head->seq_no = priv->wr_sync_counter;
        DBG("setting seq=%lli\n",  priv->wr_sync_counter);
        //3 - Again, make sure all the memory writes are done
        __sync_synchronize();

        priv->wr_sync_counter++;


        DBG("seq no   =%lli offset=(%lli)\n", curr_slot_head->seq_no, (char*)hdr_mem - (char*)priv->bring_head);
        DBG("data size=%lli\n", curr_slot_head->data_size);
    }

    return CAMIO_ENOERROR;
}


//This should queue up a new request or requests.
static camio_error_t bring_write_request(camio_stream_t* this, camio_write_req_t* req_vec, ch_word req_vec_len)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    if(priv->write_registered){
        ERR("Already registered a write request. Currently this stream only handles one outstanding request at a time\n");
        return CAMIO_EINVALID; //TODO XXX better error code
    }

    //Register the new write request
    priv->write_req         = req_vec;
    priv->write_req_len     = req_vec_len;
    priv->write_registered  = true;
    priv->write_req_curr    = 0;

    //OK now the fun begins
    DBG("Checking if write is ready\n");
    camio_error_t err = bring_write_try(this);

    return err;
}





//Is the underlying stream done writing and ready for more?
static camio_error_t bring_write_ready(camio_muxable_t* this)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        ERR("This is null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( this->mode != CAMIO_MUX_MODE_WRITE){
        ERR("Wrong kind of muxable!\n"); //WTF??
        return CAMIO_EINVALID;
    }

    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this->parent.stream);
    if(!priv->write_registered){ //No body has asked us to write anything, so we're not ready
        return CAMIO_ENOTREADY;
    }

    for(int i = priv->write_ready_curr; i < priv->write_req_len; i++ ){
        camio_write_req_t* req = priv->write_req + i;
        camio_buffer_t* buff   = req->buffer;

        //Check that the buffers are being used in order
        if(buff->__internal.__buffer_id != priv->wr_rel_index){
            ERR("Cannot use/release this buffer (id=%lli) until buffer with id=%lli is released\n",
                    buff->__internal.__buffer_id,
                    priv->wr_rel_index
            );
            return CAMIO_EWRONGBUFF;
        }

        volatile char* slot_mem                         = (volatile void*)buff->__internal.__mem_start;
        const uint64_t slot_mem_sz                      = buff->__internal.__mem_len;
        volatile char* hdr_mem                          = slot_mem + slot_mem_sz; // - sizeof(bring_slot_header_t);
        volatile bring_slot_header_t* curr_slot_head = (volatile bring_slot_header_t*)(hdr_mem);

        DBG("seq no   =%lli offset=(%lli)\n", curr_slot_head->seq_no, (char*)hdr_mem - (char*)priv->bring_head);
        DBG("data size=%lli\n", curr_slot_head->data_size);

        curr_slot_head->data_size = buff->data_len;
        //Apply an atomic update to tell the write end that we received this data
        //1 - Make sure all the memory writes are done
        __sync_synchronize();
        //2 - Do a word aligned single word write (atomic)
        if( (*(volatile uint64_t*)&curr_slot_head->seq_no) != 0x00){
            return CAMIO_ENOTREADY;
        }

        //This is an auto release stream, once you've used the buffer it goes away, you'll need to acquire another
        camio_error_t err = camio_write_release(this->parent.stream,&req->buffer);
        if(err){
            return err;
        }

        priv->write_ready_curr++;
        return CAMIO_EREADY;

    }

    //If we get here, we've successfully written all the records out. This means we're now ready to take more write requests
    DBG("Deregistering write\n");
    priv->write_registered = false;
    return CAMIO_EINVALID;

}


static camio_error_t bring_write_release(camio_stream_t* this, camio_wr_buffer_t** buffer)
{
    DBG("Doing write release\n");

    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == buffer){
        DBG("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        DBG("Buffer chain null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    camio_buffer_t* bring_buffer = *buffer;

    if( !bring_buffer->__internal.__in_use){
        DBG("Trying to release buffer that is already released\n");
        return CAMIO_ENOERROR; //Buffer has already been released
    }

    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);
    //Check that the buffers are being released in order
    if(bring_buffer->__internal.__buffer_id != priv->wr_rel_index){
        ERR("Cannot release this buffer (id=%lli) until buffer with id=%lli is released\n",
                bring_buffer->__internal.__buffer_id,
            priv->wr_rel_index
        );
        return CAMIO_EWRONGBUFF;
    }

    bring_buffer->__internal.__in_use = false;
    bring_buffer->data_len            = 0;
    bring_buffer->data_start          = NULL;
    priv->wr_rel_index++;
    if(priv->wr_rel_index >= priv->wr_buffers_count){ //But loop around
        priv->wr_rel_index = 0;
    }

    *buffer = NULL; //Remove dangling pointers!

    DBG("Done releasing write\n");

    return CAMIO_ENOERROR;
}





/**************************************************************************************************************************
 * SETUP/CLEANUP FUNCTIONS
 **************************************************************************************************************************/

static void bring_destroy(camio_stream_t* this)
{
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        DBG("This null???\n"); //WTF?
        return;
    }
    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    if(priv->bring_head){
        munlock((void*)priv->bring_head,priv->bring_head->total_mem);
        munmap((void*)priv->bring_head,priv->bring_head->total_mem);
    }

    if(this->rd_muxable.fd > -1){
        close(this->rd_muxable.fd);
        this->rd_muxable.fd = -1; //Make this reentrant safe
        this->wr_muxable.fd = -1; //Make this reentrant safe
    }

    free(this);
}



camio_error_t bring_stream_construct(
    camio_stream_t* this,
    camio_controller_t* connector,
    volatile bring_header_t* bring_head,
    bring_params_t* params,
    int fd
)
{

    DBG("Constructing bring stream\n");
    //Basic sanity checks -- TODO XXX: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        ERR("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    bring_stream_priv_t* priv = STREAM_GET_PRIVATE(this);

    priv->connector  = *connector; //Keep a copy of the connector state
    priv->params     = *params;
    priv->bring_head = bring_head;

    //Connect the read and write ends together
    priv->rd_mem            = (char*)bring_head + bring_head->rd_mem_start_offset;
    priv->rd_buffers_count  = bring_head->rd_slots;
    priv->rd_buffers        = (camio_buffer_t*)calloc(bring_head->rd_slots_size,sizeof(camio_buffer_t));
    if(!priv->rd_buffers){
        ERR("Ran out of memory allocating buffer pool\n");
        return CAMIO_ENOMEM;
    }
    for(int i = 0; i < bring_head->rd_slots;i++){
        priv->rd_buffers[i].__internal.__buffer_id       = i;
        priv->rd_buffers[i].__internal.__do_auto_release = false;
        priv->rd_buffers[i].__internal.__in_use          = false;
        priv->rd_buffers[i].__internal.__mem_start       = (char*)priv->rd_mem + bring_head->rd_slots_size * i;
        priv->rd_buffers[i].__internal.__mem_len         = bring_head->rd_slots_size - sizeof(bring_slot_header_t);
        priv->rd_buffers[i].__internal.__parent          = this;
        priv->rd_buffers[i].__internal.__pool_id         = 0;
        priv->rd_buffers[i].__internal.__read_only       = true;
    }

    priv->wr_mem            = (char*)bring_head + bring_head->wr_mem_start_offset;
    priv->wr_buffers_count  = bring_head->wr_slots;
    priv->wr_buffers        = (camio_buffer_t*)calloc(bring_head->wr_slots_size,sizeof(camio_buffer_t));
    if(!priv->wr_buffers){
        free(priv->rd_buffers);
        ERR("Ran out of memory allocating buffer pool\n");
        return CAMIO_ENOMEM;
    }
    for(int i = 0; i < bring_head->wr_slots;i++){
        priv->wr_buffers[i].__internal.__buffer_id       = i;
        priv->wr_buffers[i].__internal.__do_auto_release = true;
        priv->wr_buffers[i].__internal.__in_use          = false;
        priv->wr_buffers[i].__internal.__mem_start       = (char*)priv->wr_mem + bring_head->wr_slots_size * i;
        priv->wr_buffers[i].__internal.__mem_len         = bring_head->wr_slots_size - - sizeof(bring_slot_header_t);
        priv->wr_buffers[i].__internal.__parent          = this;
        priv->wr_buffers[i].__internal.__pool_id         = 0;
        priv->wr_buffers[i].__internal.__read_only       = true;
    }

    if(!priv->params.server){ //Swap things around to connect both ends
        ch_word tmp_buffers_count    = priv->rd_buffers_count;
        camio_buffer_t* tmp_buffers  = priv->rd_buffers;

        priv->wr_mem                 = priv->rd_mem;
        priv->wr_buffers_count       = priv->rd_buffers_count;
        priv->wr_buffers             = priv->rd_buffers;

        priv->rd_buffers_count       = tmp_buffers_count;
        priv->rd_buffers             = tmp_buffers;
    }

    priv->rd_sync_counter   = 1; //This is where we expect the first counter
    priv->wr_sync_counter   = 1;


    this->rd_muxable.mode              = CAMIO_MUX_MODE_READ;
    this->rd_muxable.parent.stream     = this;
    this->rd_muxable.vtable.ready      = bring_read_ready;
    this->rd_muxable.fd                = fd;

    this->wr_muxable.mode              = CAMIO_MUX_MODE_WRITE;
    this->wr_muxable.parent.stream     = this;
    this->wr_muxable.vtable.ready      = bring_write_ready;
    this->wr_muxable.fd                = fd;

    DBG("Done constructing BRING stream with fd=%i\n", fd);
    return CAMIO_ENOERROR;
}


NEW_STREAM_DEFINE(bring,bring_stream_priv_t)
