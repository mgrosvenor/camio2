/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_channel.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <sys/mman.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>

#include <src/devices/channel.h>
#include <src/devices/controller.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>
#include <src/api/api.h>
#include <src/camio_debug.h>
#include <deps/chaste/data_structs/circular_buffer/circular_buffer.h>

#include "bring_channel.h"
#include "bring_device.h"

/**************************************************************************************************************************
 * PER CHANNEL STATE
 **************************************************************************************************************************/
#define CAMIO_BRING_BUFFER_SIZE (4 * 1024 * 1024)
//Current (simple) BRING recv only does one buffer at a time, this could change with vectored I/O in the future.
#define CAMIO_BRING_BUFFER_COUNT (1)

typedef struct bring_channel_priv_s {
    camio_controller_t controller;
    bring_params_t params;
    volatile bring_header_t* bring_head;
    ch_bool connected;              //Is the channel currently running? Or has close been called?

    ch_cbuff_t* wr_buff_req_queue;
    ch_cbuff_t* wr_req_queue;

    //The current read buffer
    ch_cbuff_t* rd_req_queue;       //queue for inbound read requests
    volatile char* rd_mem;          //Underlying memory to support shared mem transport
    camio_buffer_t* rd_buffers;     //All buffers for the read side, basically pointers to the shared mem region
    ch_word rd_buffers_count;       //Number of buffers in the shared mem region
    ch_word rd_sync_counter;        //Synchronization counter to protect against loop around
    ch_word rd_acq_index;           //Current index for acquiring new read buffers
    ch_word rd_rel_index;           //Current index for releasing new read buffers

    volatile char* wr_mem;          //Underlying memory for the shared memory transport
    ch_bool wr_ready;               //Is the channel ready for writing (for edge triggered multiplexers)
    camio_buffer_t* wr_buffers;     //All buffers for the write side;
    ch_word wr_buffers_count;
    ch_word wr_ready_curr;

    size_t wr_bring_size;           //Size of the bring buffer
    ch_word wr_sync_counter;        //Synchronization counter
    ch_word wr_slot_size;           //Size of each slot in the ring
    ch_word wr_slot_count;          //Number of slots in the ring

    ch_word wr_buff_acq_idx;        //Current slot for getting new buffers to write
    //ch_word wr_rel_index;           //Current slot in the bring

} bring_channel_priv_t;




/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/

static camio_error_t bring_read_request(camio_channel_t* this, camio_rd_req_t* req_vec, ch_word vec_len )
{
    DBG("Doing bring read request...!\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Make sure that the request vector items match the channel properties
    for(ch_word i = 0; i < vec_len; i++){
        if(unlikely(req_vec[i].dst_offset_hint != CAMIO_READ_REQ_DST_OFFSET_NONE)){
            DBG("Could not enqueue request %i, DST_OFFSET must be NONE\n");
            return CAMIO_EINVALID;
        }
        if(unlikely(req_vec[i].src_offset_hint != CAMIO_READ_REQ_SRC_OFFSET_NONE)){
            DBG("Could not enqueue request %i, SRC_OFFSET must be NONE\n");
            return CAMIO_EINVALID;
        }
    }

    int err = 0;
    DBG("Pushing %lli items into read request queue of size %lli with %lli items in it\n",
        vec_len,
        priv->rd_req_queue->size,
        priv->rd_req_queue->count
    );
    if(unlikely( (err = cbuff_push_back_carray(priv->rd_req_queue, req_vec,vec_len)) )){
        ERR("Could not push %lli items on to queue. Error =%lli", vec_len, err);
        if(err == CH_CBUFF_TOOMANY){
            ERR("Request queue is full\n");
            return CAMIO_ETOOMANY;
        }
        return CAMIO_EINVALID;
    }

    DBG("Bring read request done\n");
    return CAMIO_ENOERROR;

}


static camio_error_t bring_read_ready(camio_muxable_t* this)
{
    //OK now the fun begins
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);


    //Is there new data -- cacluate where to look for it
    volatile char* slot_mem     = (volatile void*)priv->rd_buffers[priv->rd_acq_index].__internal.__mem_start;
    const ch_word slot_mem_sz  = priv->rd_buffers[priv->rd_acq_index].__internal.__mem_len;
    volatile char* hdr_mem      = slot_mem + slot_mem_sz;

    DBG("Doing read peek on index %lli at %p\n", priv->rd_acq_index, slot_mem, hdr_mem );

    //Make sure it's loaded in memory
    __sync_synchronize();
    volatile bring_slot_header_t curr_slot_head = *(volatile bring_slot_header_t*)(hdr_mem);
    __sync_synchronize();

    DBG("seq no=%lli data size=%lli offset=(%lli)\n",
            curr_slot_head.seq_no, curr_slot_head.data_size, (char*)hdr_mem - (char*)priv->bring_head);

    //Check if there is new data
    if( curr_slot_head.seq_no == 0){
        return CAMIO_ETRYAGAIN;
    }

    //Do some sanity checks
    if(curr_slot_head.data_size > slot_mem_sz){
        ERR("Data size is larger than memory size, corruption is likely!\n");
        return CAMIO_ENOERROR;
    }

    if( curr_slot_head.seq_no != priv->rd_sync_counter){
        DBG( "Ring overflow. This should not happen with a blocking ring %llu to %llu\n",
            curr_slot_head.seq_no,
            priv->rd_sync_counter
        );
        return CAMIO_EWRONGBUFF;
    }

    //It looks like we're good to go, try to get the new data
    camio_rd_req_t* req = cbuff_use_front(priv->rd_req_queue);
    if(!req){
        if(priv->rd_req_queue->in_use > 0){
            return CAMIO_ENOERROR; //We have data waiting, send it out!
        }
        DBG("No unsued requests are remaining, please add a new request\n");
        return CAMIO_ENOREQS;
    }

    //We have a request structure, so we can return it
    req->buffer                       = &priv->rd_buffers[priv->rd_acq_index];
    req->buffer->data_start           = (void*)slot_mem;
    req->buffer->data_len             = curr_slot_head.data_size;
    req->buffer->valid                = true;
    req->buffer->__internal.__in_use  = true;

    //And increment everything to look for the next one
    priv->rd_sync_counter++;
    priv->rd_acq_index++;   //Move to the next index
    if(priv->rd_acq_index >= priv->rd_buffers_count){ //But loop around
        priv->rd_acq_index = 0;
    }

    DBG("Got new data! acq_index=%lli buffers_count=%lli\n", priv->rd_acq_index, priv->rd_buffers_count);

    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_result( camio_channel_t* this, camio_rd_req_t** req_vec_o )
{
    DBG("Trying to get read result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(priv->rd_req_queue->_used_index <= 0){
        camio_error_t err = bring_read_ready(this);
        if(err){
            DBG("There is no data available to return, try again another time\n");
            return err;
        }
    }

    *req_vec_o = cbuff_peek_front(priv->rd_req_queue);
    cbuff_pop_front(priv->rd_req_queue);

    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_release(camio_channel_t* this, camio_rd_buffer_t** buffer)
{
    DBG("Doing read release\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
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
//Ask for write buffers
static camio_error_t bring_write_buffer_request(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len )
{
    DBG("Doing write buffer request\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Make sure that the request vector items match the channel properties
    for(ch_word i = 0; i < vec_len; i++){
        if(unlikely(req_vec[i].dst_offset_hint != CAMIO_WRITE_REQ_DST_OFFSET_NONE)){
            DBG("Could not enqueue request %i, DST_OFFSET must be NONE\n");
            return CAMIO_EINVALID;
        }
        if(unlikely(req_vec[i].src_offset_hint != CAMIO_WRITE_REQ_SRC_OFFSET_NONE)){
            DBG("Could not enqueue request %i, SRC_OFFSET must be NONE\n");
            return CAMIO_EINVALID;
        }
    }

    DBG("Pushing %lli items into write buffer request queue of size %lli with %lli items in it\n",
        vec_len,
        priv->wr_buff_req_queue->size,
        priv->wr_buff_req_queue->count
    );

    int err = 0;
    if( (err = cbuff_push_back_carray(priv->wr_buff_req_queue, req_vec,vec_len)) ){
        ERR("Could not push %lli items on to queue. Error =%lli", vec_len, err);
        if(err == CH_CBUFF_TOOMANY){
            ERR("Request queue is full\n");
            return CAMIO_ETOOMANY;
        }
        return CAMIO_EINVALID;
    }

    DBG("Done with write buffer request\n");
    return CAMIO_ENOERROR;

}


static camio_error_t bring_write_buffer_ready(camio_muxable_t* this)
{
    DBG("Doing write buffer ready\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    DBG("Trying to get buffer at index=%lli\n", priv->wr_buff_acq_idx);


    //Is there a new slot ready for writing?
    camio_buffer_t* result      = &priv->wr_buffers[priv->wr_buff_acq_idx];
    volatile char* slot_mem     = (volatile void*)result->__internal.__mem_start;
    const uint64_t slot_mem_sz  = result->__internal.__mem_len;
    volatile char* hdr_mem      = slot_mem + slot_mem_sz;

    __sync_synchronize();
    register bring_slot_header_t curr_slot_head = *(volatile bring_slot_header_t*)(hdr_mem);
    __sync_synchronize();

    DBG("seq no=%lli data size=%lli offset=(%lli)\n",
                curr_slot_head.seq_no, curr_slot_head.data_size, (char*)hdr_mem - (char*)priv->bring_head);


    if( curr_slot_head.seq_no != 0x00ULL){
        return CAMIO_ETRYAGAIN;
    }

    //It looks like we're good to go, try to get the new buffer
    camio_rd_req_t* req = cbuff_use_front(priv->wr_buff_req_queue);
    if(!req){
        if(priv->wr_buff_req_queue->in_use > 0){
            return CAMIO_ENOERROR; //We have buffers waiting, send them out!
        }
        DBG("No unused requests are remaining, please add a new request\n");
        return CAMIO_ENOREQS;
    }

    //We have a request structure, so we can return it
    req->buffer                       = result;
    req->buffer->data_start           = (void*)slot_mem;
    req->buffer->data_len             = slot_mem_sz;
    req->buffer->valid                = true;
    req->buffer->__internal.__in_use  = true;

    //And increment everything to look for the next one
    priv->wr_buff_acq_idx++;
    if(priv->wr_buff_acq_idx >= priv->wr_buffers_count){ //But loop around
        priv->wr_buff_acq_idx = 0;
    }

    DBG("Got new bufffer! acq_index=%lli buffers_count=%lli\n", priv->wr_buff_acq_idx, priv->wr_buffers_count);

    return CAMIO_ENOERROR;
}


camio_error_t bring_write_buffer_result(camio_channel_t* this, camio_wr_req_t** res_o)
{
    DBG("Getting write buffer result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(priv->wr_req_queue->_used_index <= 0){
        camio_error_t err = bring_write_buffer_ready(this);
        if(err){
            DBG("There are no new buffers available to return, try again another time\n");
            return err;
        }
    }

    *res_o = cbuff_peek_front(priv->wr_buff_req_queue);
    cbuff_pop_front(priv->wr_buff_req_queue);

    return CAMIO_ENOERROR;

}



//Try to write to the underlying, channel. This function is private, so no precondition checks necessary
static camio_error_t bring_write_try(camio_channel_t* this)
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    for(int i = priv->write_req_curr; i < priv->write_req_len; i++, priv->write_req_curr++){
        camio_wr_req_t* req = priv->write_req + i;
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
static camio_error_t bring_write_request(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len)
{
    DBG("Doing write request\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Make sure that the request vector items match the channel properties
    for(ch_word i = 0; i < vec_len; i++){
        if(unlikely(req_vec[i].dst_offset_hint != CAMIO_WRITE_REQ_DST_OFFSET_NONE)){
            DBG("Could not enqueue request %i, DST_OFFSET must be NONE\n");
            return CAMIO_EINVALID;
        }
        if(unlikely(req_vec[i].src_offset_hint != CAMIO_WRITE_REQ_SRC_OFFSET_NONE)){
            DBG("Could not enqueue request %i, SRC_OFFSET must be NONE\n");
            return CAMIO_EINVALID;
        }
    }

    DBG("Pushing %lli items into write  request queue of size %lli with %lli items in it\n",
        vec_len,
        priv->wr_req_queue->size,
        priv->wr_req_queue->count
    );

    int err = 0;
    if( (err = cbuff_push_back_carray(priv->wr_req_queue, req_vec,vec_len)) ){
        ERR("Could not push %lli items on to queue. Error =%lli", vec_len, err);
        if(err == CH_CBUFF_TOOMANY){
            ERR("Request queue is full\n");
            return CAMIO_ETOOMANY;
        }
        return CAMIO_EINVALID;
    }

    DBG("Done with write request\n");
    return CAMIO_ENOERROR;
}




//Is the underlying channel done writing and ready for more?
static camio_error_t bring_write_ready(camio_muxable_t* this)
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);
    if(!priv->write_registered){ //No body has asked us to write anything, so we're not ready
        return CAMIO_ENOTREADY;
    }

    for(int i = priv->write_ready_curr; i < priv->write_req_len; i++ ){
        camio_wr_req_t* req = priv->write_req + i;
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

        //This is an auto release channel, once you've used the buffer it goes away, you'll need to acquire another
        camio_error_t err = camio_chan_wr_buff_release(this->parent.channel,&req->buffer);
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


static camio_error_t bring_write_result(camio_muxable_t* this)
{
    DBG("Getting write buffer result\n");
    return CAMIO_NOTIMPLEMENTED;
}


static camio_error_t bring_write_buffer_release(camio_channel_t* this, camio_wr_buffer_t* buffer)
{
    DBG("Doing write release\n");

    camio_buffer_t* bring_buffer = *buffer;

    if( !bring_buffer->__internal.__in_use){
        DBG("Trying to release buffer that is already released\n");
        return CAMIO_ENOERROR; //Buffer has already been released
    }

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
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

static void bring_destroy(camio_channel_t* this)
{

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    if(priv->bring_head){
        munlock((void*)priv->bring_head,priv->bring_head->total_mem);
        munmap((void*)priv->bring_head,priv->bring_head->total_mem);
        priv->bring_head = NULL;
    }

    if(this->rd_muxable.fd > -1){
        close(this->rd_muxable.fd);
        this->rd_muxable.fd = -1; //Make this reentrant safe
        this->wr_muxable.fd = -1; //Make this reentrant safe
    }

    if(priv->rd_req_queue){
        cbuff_delete(priv->rd_req_queue);
        priv->rd_req_queue = NULL;
    }

    if(priv->wr_req_queue){
        cbuff_delete(priv->wr_req_queue);
        priv->wr_req_queue = NULL;
    }

    if(priv->wr_buff_req_queue){
        cbuff_delete(priv->wr_buff_req_queue);
        priv->wr_buff_req_queue = NULL;
    }

    free(this);
}



camio_error_t bring_channel_construct(
    camio_channel_t* this,
    camio_controller_t* controller,
    volatile bring_header_t* bring_head,
    bring_params_t* params,
    int fd
)
{

    DBG("Constructing bring channel\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    priv->controller  = *controller; //Keep a copy of the controller state
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
    this->rd_muxable.parent.channel     = this;
    this->rd_muxable.vtable.ready      = bring_read_ready;
    this->rd_muxable.fd                = fd;

    this->wr_muxable.mode              = CAMIO_MUX_MODE_WRITE;
    this->wr_muxable.parent.channel     = this;
    this->wr_muxable.vtable.ready      = bring_write_ready;
    this->wr_muxable.fd                = fd;

    //TODO: Should wire in this parameter from the outside, 1024 will do for the moment.
    priv->rd_req_queue      = ch_cbuff_new(1024,sizeof(void*));
    priv->wr_buff_req_queue = ch_cbuff_new(1024,sizeof(void*));
    priv->write_req         = ch_cbuff_new(1024,sizeof(void*));

    DBG("Done constructing BRING channel with fd=%i\n", fd);
    return CAMIO_ENOERROR;
}


NEW_CHANNEL_DEFINE(bring,bring_channel_priv_t)

