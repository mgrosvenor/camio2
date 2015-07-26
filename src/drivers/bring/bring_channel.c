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


    //Read side variables
    ch_cbuff_t* rd_req_queue;       //queue for inbound read requests
    volatile char* rd_mem;          //Underlying memory to support shared mem transport
    camio_buffer_t* rd_buffers;     //All buffers for the read side, basically pointers to the shared mem region
    ch_word rd_buffers_count;       //Number of buffers in the shared mem region
    ch_word rd_sync_counter;        //Synchronization counter to protect against loop around
    ch_word rd_acq_index;           //Current index for acquiring new read buffers
    ch_word rd_rel_index;           //Current index for releasing new read buffers

    //Write side variables
    ch_cbuff_t* wr_buff_req_queue;  //queue for inbound write buffer requests
    ch_cbuff_t* wr_req_queue;       //queue for inbound write requests
    volatile char* wr_mem;          //Underlying memory for the shared memory transport
    ch_bool wr_ready;               //Is the channel ready for writing (for edge triggered multiplexers)
    camio_buffer_t* wr_buffers;     //All buffers for the write side;
    ch_word wr_buffers_count;
    ch_word wr_ready_curr;

    ch_word wr_sync_counter;        //Synchronization counter
    ch_word wr_slot_size;           //Size of each slot in the ring
    ch_word wr_slot_count;          //Number of slots in the ring

    ch_word wr_buff_acq_idx;        //Current slot for getting new buffers to write
    ch_word wr_out_index;           //Current slot for sending data
    ch_word wr_rel_index;           //Current slot for sending data

} bring_channel_priv_t;

/**************************************************************************************************************************
 * UTLITIES
 **************************************************************************************************************************/


static inline ch_word get_head_seq(camio_buffer_t* buffers, ch_word idx)
{
    //Is there new data -- calculate where to look for it
    volatile char* slot_mem    = (volatile void*)buffers[idx].__internal.__mem_start;
    const ch_word slot_mem_sz  = buffers[idx].__internal.__mem_len;
    volatile char* hdr_mem     = slot_mem + slot_mem_sz;
    volatile bring_slot_header_t*  hdr = (volatile bring_slot_header_t*) hdr_mem;
    __sync_synchronize();
    volatile ch_word result = ~0;
    __sync_synchronize();
    result = *(volatile ch_word*)(&hdr->seq_no);
    __sync_synchronize();

    return result;
}

static inline ch_word get_head_size(camio_buffer_t* buffers, ch_word idx)
{
    //Is there new data -- calculate where to look for it
    volatile char* slot_mem    = (volatile void*)buffers[idx].__internal.__mem_start;
    const ch_word slot_mem_sz  = buffers[idx].__internal.__mem_len;
    volatile char* hdr_mem     = slot_mem + slot_mem_sz;
    volatile bring_slot_header_t*  hdr = (volatile bring_slot_header_t*) hdr_mem;
    __sync_synchronize();
    volatile ch_word result = ~0;
    __sync_synchronize();
    result = *(volatile ch_word*)(&hdr->data_size);
    __sync_synchronize();

    return result;
}



static inline void set_head(camio_buffer_t* buffers, ch_word idx, ch_word seq_no, ch_word data_size)
{
    DBG("Setting header at %lli seq no=%lli data size=%lli\n", idx, seq_no, data_size);

    //Is there new data -- calculate where to look for it
    volatile char* slot_mem    = (volatile void*)buffers[idx].__internal.__mem_start;
    const ch_word slot_mem_sz  = buffers[idx].__internal.__mem_len;
    volatile char* hdr_mem     = slot_mem + slot_mem_sz;
    volatile bring_slot_header_t*  hdr = (volatile bring_slot_header_t*) hdr_mem;
    __sync_synchronize();
    *(volatile ch_word*)(&hdr->data_size) = data_size;
    __sync_synchronize();
    *(volatile ch_word*)(&hdr->seq_no) = seq_no;
    __sync_synchronize();

    //ERR("%p / (%lli)\n", &((volatile bring_slot_header_t*)(hdr_mem))->seq_no, (size_t)(&(((volatile bring_slot_header_t*)(hdr_mem))->seq_no)) % 8 );

}


/**************************************************************************************************************************
 * READ FUNCTIONS
 **************************************************************************************************************************/

static camio_error_t bring_read_request(camio_channel_t* this, camio_rd_req_t* req_vec, ch_word vec_len )
{
    //DBG("Doing bring read request...!\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    int err = 0;
    /*DBG("Pushing %lli items into read request queue of size %lli with %lli items in it\n",
        vec_len,
        priv->rd_req_queue->size,
        priv->rd_req_queue->count
    );*/
    if(unlikely( (err = cbuff_push_back_carray(priv->rd_req_queue, &req_vec,vec_len)) )){
        ERR("Could not push %lli items on to queue. Error =%lli", vec_len, err);
        if(err == CH_CBUFF_TOOMANY){
            ERR("Request queue is full\n");
            return CAMIO_ETOOMANY;
        }
        return CAMIO_EINVALID;
    }

    //DBG("Bring read request done\n");
    return CAMIO_ENOERROR;

}



static camio_error_t bring_read_ready(camio_muxable_t* this)
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);
    //DBG("Doing read ready\n");

    //Try to fill as many read requests as we can
    camio_rd_req_t** req_p = NULL;
    for( req_p = cbuff_use_front(priv->rd_req_queue); req_p != NULL; req_p = cbuff_use_front(priv->rd_req_queue)){
        camio_rd_req_t* req = *req_p;

        //Check that the request is a valid one
        if(unlikely(req->dst_offset_hint != CAMIO_READ_REQ_DST_OFFSET_NONE)){
            ERR("Could not enqueue request %i, DST_OFFSET must be NONE\n");
            req->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }
        if(unlikely(req->src_offset_hint != CAMIO_READ_REQ_SRC_OFFSET_NONE)){
            ERR("Could not enqueue request %i, SRC_OFFSET must be NONE\n");
            req->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }

        //This is the atomic get operation
        volatile ch_word seq_no = get_head_seq(priv->rd_buffers,priv->rd_acq_index);

        //Check if there is new data
        if(likely(seq_no == 0)){
            break; //There is no data available. Exit the loop which will return ETRYAGAIN
        }

        volatile ch_word data_size = get_head_size(priv->rd_buffers,priv->rd_acq_index);


        if(data_size == 0){
            DBG("1 New slot_head->seq=%lli ->len=%lli\n", seq_no, data_size);
            while(data_size != 0){
                DBG("2 New slot_head->seq=%lli ->len=%lli\n", seq_no, data_size);
                data_size = get_head_size(priv->rd_buffers,priv->rd_acq_index);
                __asm__ __volatile__("pause;");
            }
            DBG("3 New slot_head->seq=%lli ->len=%lli\n", seq_no, data_size);
            data_size = get_head_size(priv->rd_buffers,priv->rd_acq_index);
            DBG("3 New slot_head->seq=%lli ->len=%lli\n", seq_no, data_size);
            exit(1);
        }

        //Do some sanity checks
        if(unlikely(data_size > priv->rd_buffers[priv->rd_acq_index].__internal.__mem_len)){
            ERR("Data size is larger than memory size, corruption is likely!\n");
            req->status = CAMIO_ETOOMANY; //TODO better error code here!
            break;
        }

        if(unlikely( seq_no != priv->rd_sync_counter)){
            DBG( "Ring synchronization error. This should not happen with a blocking ring %llu to %llu\n",
                seq_no,
                priv->rd_sync_counter
            );
            return CAMIO_EWRONGBUFF;
        }


        //We have a request structure, it is valid, and we have a readable item, so we can return it!
        req->buffer                       = &priv->rd_buffers[priv->rd_acq_index];
        req->buffer->data_start           = priv->rd_buffers[priv->rd_acq_index].__internal.__mem_start;
        req->buffer->data_len             = data_size;
        req->buffer->valid                = true;
        req->buffer->__internal.__in_use  = true;

        //And increment everything to look for the next one
        priv->rd_sync_counter++;
        priv->rd_acq_index++;   //Move to the next index
        if(priv->rd_acq_index >= priv->rd_buffers_count){ //But loop around
            priv->rd_acq_index = 0;
        }

        DBG("Got new data size = %lli acq_index=%lli buffers_count=%lli\n",
                data_size, priv->rd_acq_index, priv->rd_buffers_count);

        //Continue around the loop here
    }

    if(req_p != NULL){
        //DBG("Freeing unused request\n");
        cbuff_unuse_front(priv->rd_req_queue);
    }

    if(priv->rd_req_queue->in_use == 0){
        //DBG("There is nothing ready to read\n");
        return CAMIO_ETRYAGAIN;
    }

    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_result( camio_channel_t* this, camio_rd_req_t** res_o )
{
    DBG("Trying to get read result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->rd_req_queue->in_use <= 0)){
        camio_error_t err = bring_read_ready(&this->rd_muxable);
        if(err){
            ERR("There is no data available to return. Did you use read_ready()?\n");
            return err;
        }
    }

    camio_rd_req_t** req_p = cbuff_peek_front(priv->rd_req_queue);
    *res_o = *req_p;
    cbuff_unuse_front(priv->rd_req_queue);
    cbuff_pop_front(priv->rd_req_queue);

    DBG("Returning read result data_start=%p, data_size=%lli\n", (*res_o)->buffer->data_start, (*res_o)->buffer->data_len );

    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_release(camio_channel_t* this, camio_rd_buffer_t* buffer)
{
    //DBG("Doing read release\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Check that the buffers are being freed in order
    if(buffer->__internal.__buffer_id != priv->rd_rel_index){
        ERR("Cannot release this buffer (id=%lli) until buffer with id=%lli is released\n",
            buffer->__internal.__buffer_id,
            priv->rd_rel_index);
        return CAMIO_EWRONGBUFF;
    }

    //DBG("Trying to release buffer at index=%lli\n", priv->rd_rel_index);
    set_head(priv->rd_buffers,priv->rd_rel_index,0,0xFF);

    buffer->__internal.__in_use   = false;
    buffer->valid                 = false;

    //We're done. Increment the buffer index and wrap around if necessary -- this is faster than using a modulus (%)
    priv->rd_rel_index++;
    if(priv->rd_rel_index >= priv->rd_buffers_count){
        priv->rd_rel_index = 0;
    }

    //DBG("Done releasing buffer!\n");

    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS - REQUEST BUFFERS
 **************************************************************************************************************************/
//Ask for write buffers
static camio_error_t bring_write_buffer_request(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len )
{
   // DBG("Doing write buffer request\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

   /* DBG("Pushing %lli items into write buffer request queue of size %lli with %lli items in it\n",
        vec_len,
        priv->wr_buff_req_queue->size,
        priv->wr_buff_req_queue->count
    );*/

    int err = 0;
    if( (err = cbuff_push_back_carray(priv->wr_buff_req_queue, &req_vec,vec_len)) ){
        ERR("Could not push %lli items on to queue. Error =%lli", vec_len, err);
        if(err == CH_CBUFF_TOOMANY){
            ERR("Request queue is full\n");
            return CAMIO_ETOOMANY;
        }
        return CAMIO_EINVALID;
    }

    //DBG("Done with write buffer request\n");
    return CAMIO_ENOERROR;

}


static camio_error_t bring_write_buffer_ready(camio_muxable_t* this)
{
    //DBG("Doing write buffer ready\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);

    camio_wr_req_t** req_p = cbuff_use_front(priv->wr_buff_req_queue);
    for( ; req_p != NULL; req_p = cbuff_use_front(priv->wr_buff_req_queue)){
        camio_wr_req_t* req = *req_p;

        //DBG("Trying to get buffer at index=%lli\n", priv->wr_buff_acq_idx);

        volatile ch_word seq_no = get_head_seq(priv->wr_buffers, priv->wr_buff_acq_idx);

        if( seq_no != 0x00ULL){
            break;
        }

        //We have a request structure and a valid buffer, so we can return it
        req->buffer                       = &priv->wr_buffers[priv->wr_buff_acq_idx];
        req->buffer->data_start           = priv->wr_buffers[priv->wr_buff_acq_idx].__internal.__mem_start;
        req->buffer->data_len             = priv->wr_buffers[priv->wr_buff_acq_idx].__internal.__mem_len;
        req->buffer->valid                = true;
        req->buffer->__internal.__in_use  = true;

        DBG("Got new buffer! acq_index=%lli buffers_count=%lli\n", priv->wr_buff_acq_idx, priv->wr_buffers_count);

        //And increment everything to look for the next one
        priv->wr_buff_acq_idx++;
        if(priv->wr_buff_acq_idx >= priv->wr_buffers_count){ //But loop around
            priv->wr_buff_acq_idx = 0;
        }
    }

    if(req_p != NULL){
        //DBG("Freeing unused request\n");
        cbuff_unuse_front(priv->wr_buff_req_queue);
    }
    if(priv->wr_buff_req_queue->in_use == 0){
        //DBG("There are no buffers available\n");
        return CAMIO_ETRYAGAIN;
    }

    //DBG("Current %lli new write requests outstanding to be consumed\n", priv->wr_buff_req_queue->in_use);
    return CAMIO_ENOERROR;
}


camio_error_t bring_write_buffer_result(camio_channel_t* this, camio_wr_req_t** res_o)
{
    //DBG("Getting write buffer result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(priv->wr_buff_req_queue->in_use <= 0){
        camio_error_t err = bring_write_buffer_ready(&this->wr_buff_muxable);
        if(err){
            DBG("There are no new buffers available to return, try again another time\n");
            return err;
        }
    }

    camio_wr_req_t** res = cbuff_peek_front(priv->wr_buff_req_queue);
    *res_o = *res;
    cbuff_unuse_front(priv->wr_buff_req_queue);
    cbuff_pop_front(priv->wr_buff_req_queue);

    //DBG("Returning a new write buffer. Data starts at %p\n", (*res_o)->buffer->data_start);
    return CAMIO_ENOERROR;

}



static camio_error_t bring_write_buffer_release(camio_channel_t* this, camio_wr_buffer_t* buffer)
{
    //DBG("Doing write buffer release\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    //Check that the buffers are being released in order
    if(buffer->__internal.__buffer_id != priv->wr_rel_index){
        ERR("Cannot release this buffer (id=%lli) until buffer with id=%lli is released\n",
                buffer->__internal.__buffer_id,
            priv->wr_rel_index
        );
        return CAMIO_EWRONGBUFF;
    }

    buffer->__internal.__in_use = false;
    buffer->data_len            = -14;
    buffer->data_start          = (void*)0xFF;
    priv->wr_rel_index++;
    if(priv->wr_rel_index >= priv->wr_buffers_count){ //But loop around
        priv->wr_rel_index = 0;
    }

    //DBG("Done releasing write buffer\n");

    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS - WRITE DATA
 **************************************************************************************************************************/
//This should queue up a new request or requests. In this case, since writes simply require some checks and a bit flip
//the actual "write" is going to happen here
static camio_error_t bring_write_request(camio_channel_t* this, camio_wr_req_t* req_vec, ch_word vec_len)
{
   // DBG("Doing write request\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //DBG("Trying to push %lli items into write  request queue of size %lli with %lli items in it\n",
    //    vec_len,
    //    priv->wr_req_queue->size,
    //    priv->wr_req_queue->count
    //);

    if(priv->wr_req_queue->count + vec_len > priv->wr_req_queue->size){
        ERR("Request queue is full\n");
        return CAMIO_ETOOMANY;
    }

    //Try to perform the write requests
    for(ch_word i = 0; i < vec_len; i++){

        camio_wr_req_t* req =  &req_vec[i];
        camio_buffer_t* buff = req->buffer;

        //DBG("Trying to process write data request of size %lli with data start=%p index=%lli\n",
        //        req->buffer->data_len, req->buffer->data_start, req->buffer->__internal.__buffer_id);

        req->status = 0; //Reset this status. We will use it again later

        int err = 0;
        if( (err = cbuff_push_back(priv->wr_req_queue,&req)) ){
            ERR("Could not push items on to queue even though there is space??. Error =%lli", err);
            return CAMIO_EINVALID; //Exit now, this is unrecoverable
        }

        if(unlikely(req->dst_offset_hint != CAMIO_WRITE_REQ_DST_OFFSET_NONE)){
            ERR("Could not complete request %i, DST_OFFSET must be NONE\n");
            req->status = CAMIO_EINVALID; //TODO XXX get a better error code
            continue;
        }
        if(unlikely(req->src_offset_hint != CAMIO_WRITE_REQ_SRC_OFFSET_NONE)){
            ERR("Could not complete request %i, SRC_OFFSET must be NONE\n");
            req->status = CAMIO_EINVALID; //TODO XXX get a better error code
            continue;
        }

        //Check that the buffers are being used in order
        if(buff->__internal.__buffer_id != priv->wr_out_index){
            ERR("Cannot use this buffer (id=%lli) until buffer with id=%lli is used\n",
                buff->__internal.__buffer_id,
                priv->wr_out_index
            );
            req->status = CAMIO_EWRONGBUFF;
            continue;
        }

        //Check that the data is not corrupted
        if(buff->data_len > buff->__internal.__mem_len){
            ERR("Data length (%lli) is longer than buffer length (%lli), corruption has probably occured\n",
                buff->data_len,
                buff->__internal.__mem_len
            );
            req->status = CAMIO_ETOOMANY;
            return CAMIO_EINVALID; //Exit now, this is unrecoverable
        }

        //OK. Looks like the request is ok, do we have space for it, we should do!
        //Make the write visible to the read side
        set_head(priv->wr_buffers,priv->wr_out_index,priv->wr_sync_counter,buff->data_len);

        DBG("Write data request to idx=%lli of size %lli with data start=%p sent with seq=%lli\n",
                priv->wr_out_index, req->buffer->data_len, req->buffer->data_start, priv->wr_sync_counter);

        priv->wr_sync_counter++;
        priv->wr_out_index++;
        if(priv->wr_out_index >= (ch_word)priv->wr_buffers_count){
            priv->wr_out_index = 0;
        }
    }

    //DBG("Done adding new write requests there are %lli requests in flight\n", priv->wr_sync_counter -1);
    return CAMIO_ENOERROR;
}



//Is the underlying channel done writing and ready for more?
static camio_error_t bring_write_ready(camio_muxable_t* this)
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);

    camio_wr_req_t** req_p = NULL;
    for( req_p = cbuff_use_front(priv->wr_req_queue); req_p != NULL; req_p = cbuff_use_front(priv->wr_req_queue)){

        camio_wr_req_t* req = *req_p;
        const ch_word buff_idx = req->buffer->__internal.__buffer_id;

        //DBG("Checking if buffer at index=%lli is ready\n", buff_idx);

        if(req->status){
            //An error was detected, so there's no point in going on with this request.
            continue;
        }

        volatile ch_word seq_no = get_head_seq(priv->wr_buffers, buff_idx);

        if( seq_no != 0x00ULL){
            break;
        }

        volatile ch_word data_size = get_head_size(priv->wr_buffers, buff_idx);
        DBG("Buffer at index=%lli is ready with %lli bytes written!\n", buff_idx, data_size);
        (void)data_size;
        //This is an auto release channel, once buffer is written out, it goes away, you'll need to acquire another
        camio_error_t err = camio_chan_wr_buff_release(this->parent.channel,req->buffer);
        if(err){
            return err;
        }

        req->buffer = NULL;
        req->status = CAMIO_ENOERROR;
    }

    if(req_p != NULL){
        //DBG("Freeing unused request\n");
        cbuff_unuse_front(priv->wr_req_queue);
    }

    if(priv->wr_req_queue->in_use == 0){
        //DBG("There are no buffers yet available\n");
        return CAMIO_ETRYAGAIN;
    }

    //DBG("Currently %lli write requests ready to be consumed\n", priv->wr_req_queue->in_use);
    return CAMIO_ENOERROR;

}


static camio_error_t bring_write_result(camio_channel_t* this, camio_wr_req_t** res_o)
{
   // DBG("Getting write buffer result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(priv->wr_req_queue->in_use <= 0){
        camio_error_t err = bring_write_buffer_ready(&this->wr_muxable);
        if(err){
            ERR("There are no new buffers available to return, try again another time\n");
            return err;
        }
    }

    camio_wr_req_t** res = cbuff_peek_front(priv->wr_req_queue);
    *res_o = *res;
    cbuff_unuse_front(priv->wr_req_queue);
    cbuff_pop_front(priv->wr_req_queue);

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
    priv->rd_buffers        = (camio_buffer_t*)calloc(bring_head->rd_slots,sizeof(camio_buffer_t));
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
    priv->wr_buffers        = (camio_buffer_t*)calloc(bring_head->wr_slots,sizeof(camio_buffer_t));
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


    //TODO: Should wire in this parameter from the outside, 1024 will do for the moment.
    priv->rd_req_queue      = ch_cbuff_new(priv->rd_buffers_count,sizeof(void*));
    priv->wr_buff_req_queue = ch_cbuff_new(priv->wr_buffers_count,sizeof(void*));
    priv->wr_req_queue      = ch_cbuff_new(priv->wr_buffers_count,sizeof(void*));



    (void)fd;
   // DBG("Done constructing BRING channel with fd=%i\n", fd);
    //exit(1);
    return CAMIO_ENOERROR;


}


NEW_CHANNEL_DEFINE(bring,bring_channel_priv_t)

