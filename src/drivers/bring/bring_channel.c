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
#include <deps/chaste/data_structs/circular_buffer/circular_buffer.h>

#include "bring_channel.h"
#include "bring_device.h"

/**************************************************************************************************************************
 * PER CHANNEL STATE
 **************************************************************************************************************************/
//#define CAMIO_BRING_BUFFER_SIZE (4 * 1024 * 1024)
//Current (simple) BRING recv only does one buffer at a time, this could change with vectored I/O in the future.
//#define CAMIO_BRING_BUFFER_COUNT (1)


#define BRING_SYNC_START 0x2ULL
#define BRING_SYNC_BUFF_EMPTY 0x0ULL
#define BRING_SYNC_BUFF_RXD   0x1ULL

typedef struct bring_channel_priv_s {
    camio_controller_t controller;
    bring_params_t params;
    volatile bring_header_t* bring_head;
    ch_bool connected;              //Is the channel currently running? Or has close been called?


    //Read side variables
    ch_cbuff_t* rd_buff_q;          //queue for inbound read buffer requests
    ch_cbuff_t* rd_data_q;          //queue for inbound read requests
    volatile char* rd_mem;          //Underlying memory to support shared mem transport
    camio_buffer_t* rd_buffs;       //All buffers for the read side, basically pointers to the shared mem region
    ch_word rd_buffs_count;         //Number of buffers in the shared mem region
    ch_word rd_sync_counter;        //Synchronization counter to protect against loop around
    ch_word rd_acq_index;           //Current index for acquiring new read buffers
    ch_word rd_rel_index;           //Current index for releasing new read buffers

    //Write side variables
    ch_cbuff_t* wr_buff_q;          //queue for inbound write buffer requests
    ch_cbuff_t* wr_data_q;          //queue for inbound write requests
    volatile char* wr_mem;          //Underlying memory for the shared memory transport
    ch_bool wr_ready;               //Is the channel ready for writing (for edge triggered multiplexers)
    camio_buffer_t* wr_buffs;       //All buffers for the write side;
    ch_word wr_buffs_count;
    ch_word wr_ready_curr;

    ch_word wr_sync_counter;        //Synchronization counter. The assumptions is that this will never wrap around.
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
    //__sync_synchronize();
    volatile ch_word result = ~0;
    //__sync_synchronize();
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
    //__sync_synchronize();
    volatile ch_word result = ~0;
    //__sync_synchronize();
    result = *(volatile ch_word*)(&hdr->data_size);
    //__sync_synchronize();

    return result;
}


static inline void set_head(camio_buffer_t* buffers, ch_word idx, ch_word seq_no, ch_word data_size)
{
    //DBG("Setting header at %lli seq no=%lli data size=%lli\n", idx, seq_no, data_size);

    //Is there new data -- calculate where to look for it
    volatile char* slot_mem    = (volatile void*)buffers[idx].__internal.__mem_start;
    const ch_word slot_mem_sz  = buffers[idx].__internal.__mem_len;
    volatile char* hdr_mem     = slot_mem + slot_mem_sz;
    volatile bring_slot_header_t*  hdr = (volatile bring_slot_header_t*) hdr_mem;
    //__sync_synchronize();
    *(volatile ch_word*)(&hdr->data_size) = data_size;
    //__sync_synchronize();
    *(volatile ch_word*)(&hdr->seq_no) = seq_no;
    __sync_synchronize();

    //ERR("%p / (%lli)\n", &((volatile bring_slot_header_t*)(hdr_mem))->seq_no, (size_t)(&(((volatile bring_slot_header_t*)(hdr_mem))->seq_no)) % 8 );

}


/**************************************************************************************************************************
 * READ FUNCTIONS - READ BUFFER REQUEST
 **************************************************************************************************************************/
static camio_error_t bring_read_buffer_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io )
{
    DBG("Doing bring read buffer request...!\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    if(unlikely(NULL == cbuff_push_back_carray(priv->rd_buff_q, req_vec,vec_len_io))){
        ERR("Could not push any items on to queue.");
        return CAMIO_EINVALID;
    }

    //DBG("Bring read buffer request done - %lli requests added\n", *vec_len_io);
    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_buffer_ready(camio_muxable_t* this)
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);
    //DBG("Doing read buffer ready\n");

    //Try to fill as many read requests as we can
    camio_msg_t* msg = cbuff_use_front(priv->rd_buff_q);
    for(; msg != NULL; msg = cbuff_use_front(priv->rd_buff_q)){

        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_READ_BUFF_REQ)){
            ERR("Expected a read buffer request message (%i) but got %i instead.\n",CAMIO_MSG_TYPE_READ_BUFF_REQ, msg->type);
            continue;
        }

        //Extract the request and response pointers, convert this message to a response
        camio_rd_buff_req_t* req = &msg->rd_buff_req;

        //Check that the request is a valid one
        if(unlikely(req->dst_offset_hint != CAMIO_READ_REQ_DST_OFFSET_NONE)){
            ERR("Could not enqueue request %lli, DST_OFFSET must be NONE\n");
            msg->type = CAMIO_MSG_TYPE_READ_BUFF_RES;
            camio_rd_buff_res_t* res = &msg->rd_buff_res;
            res->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }
        if(unlikely(req->src_offset_hint != CAMIO_READ_REQ_SRC_OFFSET_NONE)){
            ERR("Could not enqueue request %lli, SRC_OFFSET must be NONE\n");
            msg->type = CAMIO_MSG_TYPE_READ_BUFF_RES;
            camio_rd_buff_res_t* res = &msg->rd_buff_res;
            res->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }

        //This is the atomic get operation
        volatile ch_word seq_no = get_head_seq(priv->rd_buffs,priv->rd_acq_index);

        //Check if there is new data
        if(likely(seq_no == BRING_SYNC_BUFF_EMPTY || seq_no == BRING_SYNC_BUFF_RXD )){
            break; //There is no data available. Exit the loop which will return ETRYAGAIN
        }

        if(unlikely( seq_no != priv->rd_sync_counter)){
            DBG( "Ring synchronization error. This should not happen with a blocking ring %llu to %llu\n",
                seq_no,
                priv->rd_sync_counter
            );
            msg->type = CAMIO_MSG_TYPE_READ_BUFF_RES;
            camio_rd_buff_res_t* res = &msg->rd_buff_res;
            res->status = CAMIO_EWRONGBUFF; //TODO better error code here!
            return CAMIO_EWRONGBUFF;
        }

        //From this point onwards, we're going to return something.
        msg->type = CAMIO_MSG_TYPE_READ_BUFF_RES;
        camio_rd_buff_res_t* res = &msg->rd_buff_res;

        volatile ch_word data_size = get_head_size(priv->rd_buffs,priv->rd_acq_index);
        //Do some sanity checks
        if(unlikely(data_size > priv->rd_buffs[priv->rd_acq_index].__internal.__mem_len)){
            ERR("Data size is larger than memory size, corruption is likely!\n");
            res->status = CAMIO_ETOOMANY; //TODO better error code here!
            break;
        }

        //We have a request structure, it is valid, and we have a readable item, so we can return it!
        res->buffer                       = &priv->rd_buffs[priv->rd_acq_index];
        res->buffer->data_start           = priv->rd_buffs[priv->rd_acq_index].__internal.__mem_start;
        res->buffer->data_len             = data_size;
        res->buffer->valid                = true;
        res->buffer->__internal.__in_use  = true;

        DBG("New data ready! size = %lli acq_index=%lli buffers_count=%lli result msg=%p msg_typ=%i\n",
                data_size, priv->rd_acq_index, priv->rd_buffs_count, msg, msg->type);

        //And increment everything to look for the next one
        priv->rd_sync_counter++;
        priv->rd_acq_index++;   //Move to the next index
        if(unlikely(priv->rd_acq_index >= priv->rd_buffs_count)){ //But loop around
            priv->rd_acq_index = 0;
        }
    }

    if(likely(msg != NULL)){
        cbuff_unuse_front(priv->rd_buff_q);
    }

    if(likely(priv->rd_buff_q->in_use == 0)){
        //DBG("There is nothing ready to read\n");
        return CAMIO_ETRYAGAIN;
    }

    //DBG("There are %lli new buffers to read into\n", priv->rd_buff_q->in_use);
    return CAMIO_ENOERROR;
}

static camio_error_t bring_read_buffer_result( camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io )
{
    DBG("Getting read buffer result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->rd_buff_q->in_use <= 0)){
        camio_error_t err = bring_read_buffer_ready(&this->rd_data_muxable);
        if(err){
            ERR("There is no data available to return. Did you use read_ready()?\n");
            return err;
        }
    }

    const ch_word count = MIN(priv->rd_buff_q->in_use, *vec_len_io);
    *vec_len_io = count;
    for(ch_word i = 0; i < count; i++, cbuff_pop_front(priv->rd_buff_q) ){

        camio_msg_t* msg = cbuff_peek_front(priv->rd_buff_q);
        res_vec[i] = *msg;
        camio_rd_buff_res_t* res = &res_vec[i].rd_buff_res;

        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            res_vec[i].type = CAMIO_MSG_TYPE_IGNORE;
            res->status = CAMIO_EINVALID;
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_READ_BUFF_RES)){
            ERR("[%lli] Message %p - expected a read buffer response (%i) but got %i instead.\n",
                    i, msg, CAMIO_MSG_TYPE_READ_BUFF_RES, msg->type);

            res_vec[i].type = CAMIO_MSG_TYPE_IGNORE;
            res->status = CAMIO_EINVALID;
            continue;
        }

        res->status = CAMIO_ENOERROR;
        //DBG("Returning buffer result of size=%lli acq_index=%lli/%lli result msg=%p msg_typ=%i\n",
        //        res->buffer->data_len, res->buffer->__internal.__buffer_id, priv->rd_buffs_count, msg, msg->type);

    }

    //DBG("There are now %lli read buffers ready for reading\n", count);
    return CAMIO_ENOERROR;
}



static camio_error_t bring_read_buffer_release(camio_channel_t* this, camio_buffer_t* buffer)
{
    //DBG("Doing read release\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Check that the buffers are being freed in order
    if(unlikely(buffer->__internal.__buffer_id != priv->rd_rel_index)){
        ERR("Cannot release this buffer (id=%lli) until buffer with id=%lli is released\n",
            buffer->__internal.__buffer_id,
            priv->rd_rel_index);
        return CAMIO_EWRONGBUFF;
    }

    //DBG("Trying to release buffer at index=%lli\n", priv->rd_rel_index);
    set_head(priv->rd_buffs,priv->rd_rel_index,BRING_SYNC_BUFF_RXD,buffer->data_len);

    buffer->__internal.__in_use   = false;
    buffer->valid                 = false;

    //We're done. Increment the buffer index and wrap around if necessary -- this is faster than using a modulus (%)
    priv->rd_rel_index++;
    if(unlikely(priv->rd_rel_index >= priv->rd_buffs_count)){
        priv->rd_rel_index = 0;
    }

    //DBG("Done releasing buffer!\n");
    return CAMIO_ENOERROR;
}


/**************************************************************************************************************************
 * READ FUNCTIONS - READ DATA REQUESTS
 **************************************************************************************************************************/


static camio_error_t bring_read_data_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io )
{
    //DBG("Doing bring read request...!\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    if(unlikely(NULL == cbuff_push_back_carray(priv->rd_data_q, req_vec,vec_len_io))){
        ERR("Could not push any items on to queue.");
        return CAMIO_EINVALID;
    }

    //DBG("Bring read data request done - %lli requests added\n", *vec_len_io);
    return CAMIO_ENOERROR;
}


//When we enter this function, we can assume that we have already acquired the data buffer in the a call to
//read_buffer_request. This means that all we need to do is check that the buffer that came in, belonged to us. If so, simply
//forward the buffer back out to the user. This complicated little dance is necessary to make other transports work.
//In theory, we could make this function actually take buffers from the outside world by acquiring an internal buffer and
//copying. There is a bit of dancing required to get around the async calls so I'll defer this work to a TODO XXX.
static camio_error_t bring_read_data_ready(camio_muxable_t* this)
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);
    //DBG("Doing read ready\n");

    //Try to fill as many read requests as we can
    camio_msg_t* msg = cbuff_use_front(priv->rd_data_q);
    for(; msg != NULL; msg = cbuff_use_front(priv->rd_data_q)){

        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_READ_DATA_REQ)){
            ERR("Expected a read data request message (%i) but got %lli instead.\n",CAMIO_MSG_TYPE_READ_DATA_REQ, msg->type);
            continue;
        }

        camio_rd_data_req_t* req = &msg->rd_data_req;
        camio_rd_data_res_t* res = &msg->rd_data_res;
        msg->type = CAMIO_MSG_TYPE_READ_DATA_RES;

        //Check that the request is a valid one
        if(unlikely(req->buffer == NULL)){
            ERR("Cannot use a NULL buffer. You need to call read_buffer_request first\n");
            res->status = CAMIO_EINVALID;
            continue;
        }

        if(unlikely(req->buffer->__internal.__parent != this->parent.channel)){
            ERR("Cannot use a buffer that does not belong to us!\n"); //TODO XXX -- could copy one?
            res->status = CAMIO_EWRONGBUFF; //TODO better error code here!
            continue;
        }

        if(unlikely(req->dst_offset_hint != CAMIO_READ_REQ_DST_OFFSET_NONE)){
            ERR("Could not enqueue request %i, DST_OFFSET must be NONE\n");
            res->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }
        if(unlikely(req->src_offset_hint != CAMIO_READ_REQ_SRC_OFFSET_NONE)){
            ERR("Could not enqueue request %i, SRC_OFFSET must be NONE\n");
            res->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }

        //DBG("Data is ready on msg=%p\n", msg);
        res->status = CAMIO_ENOERROR; //TODO better error code here!
    }

    if(likely(msg != NULL)){
        cbuff_unuse_front(priv->rd_data_q);
    }

    if(likely(priv->rd_data_q->in_use == 0)){
        return CAMIO_ETRYAGAIN;
    }

    //DBG("There are now %lli new read datas available\n", priv->rd_data_q->in_use);
    return CAMIO_ENOERROR;
}


static camio_error_t bring_read_data_result( camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io )
{
    //DBG("Trying to get read result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->rd_data_q->in_use <= 0)){
        camio_error_t err = bring_read_data_ready(&this->rd_data_muxable);
        if(err){
            ERR("There is no data available to return. Did you use read_ready()?\n");
            return err;
        }
    }

    const ch_word count = MIN(*vec_len_io, priv->rd_data_q->in_use);
    *vec_len_io = count;
    for(ch_word i = 0; i < count; i++, cbuff_pop_front(priv->rd_data_q)){
        camio_msg_t* msg = cbuff_peek_front(priv->rd_data_q);
        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_READ_DATA_RES)){
            ERR("Expected a read data response (%lli) but got %lli instead.\n",CAMIO_MSG_TYPE_READ_DATA_RES, msg->type);
            continue;
        }

        res_vec[i] = *msg;
        camio_rd_data_res_t* res = &res_vec[i].rd_data_res;

        if(likely(res->status == CAMIO_ENOERROR)){
            DBG("Returning read result data_start=%p, data_size=%lli\n", res->buffer->data_start, res->buffer->data_len );
        }
        else{
            DBG("Error returning read data result. Err=%i\n", msg->rd_data_res.status);
        }

        res->status = CAMIO_ECANNOTREUSE; //Let the user know that this is a once only buffer, it cannot be reused
    }

    //DBG("Returning %lli read data completions\n", count);
    return CAMIO_ENOERROR;
}




/**************************************************************************************************************************
 * WRITE FUNCTIONS - REQUEST BUFFERS
 **************************************************************************************************************************/

//Ask for write buffers
static camio_error_t bring_write_buffer_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io )
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    //DBG("Doing write buffer request. There are currently %lli items in the queue\n", priv->wr_buff_q->count);

    if(unlikely(NULL == cbuff_push_back_carray(priv->wr_buff_q, req_vec,vec_len_io))){
        ERR("Could not push any items on to queue.");
        return CAMIO_EINVALID;
    }

    //DBG("Done with write buffer request - %lli requests added\n", *vec_len_io);
    return CAMIO_ENOERROR;
}


static camio_error_t bring_write_buffer_ready(camio_muxable_t* this)
{
    //DBG("Doing write buffer ready\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);

    camio_msg_t* msg = cbuff_use_front(priv->wr_buff_q);
    for( ; msg != NULL; msg = cbuff_use_front(priv->wr_buff_q)){

        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_WRITE_BUFF_REQ)){
            ERR("Expected a write buffer request (%lli) but got %lli instead.\n",CAMIO_MSG_TYPE_WRITE_BUFF_REQ, msg->type);
            continue;
        }

        //DBG("Trying to get buffer at index=%lli\n", priv->wr_buff_acq_idx);
        volatile ch_word seq_no = get_head_seq(priv->wr_buffs, priv->wr_buff_acq_idx);

        if(likely( seq_no != BRING_SYNC_BUFF_EMPTY)){
            break;
        }

        //We have a request structure and a valid buffer, so we can return it
        camio_wr_buff_res_t* res = &msg->wr_buff_res;
        msg->type = CAMIO_MSG_TYPE_WRITE_BUFF_RES;
        res->buffer                       = &priv->wr_buffs[priv->wr_buff_acq_idx];
        res->buffer->data_start           = priv->wr_buffs[priv->wr_buff_acq_idx].__internal.__mem_start;
        res->buffer->data_len             = priv->wr_buffs[priv->wr_buff_acq_idx].__internal.__mem_len;
        res->buffer->valid                = true;
        res->buffer->__internal.__in_use  = true;
        res->status = CAMIO_ENOERROR;
        //DBG("Got a free buffer! acq_index=%lli buffers_count=%lli\n", priv->wr_buff_acq_idx, priv->wr_buffers_count);

        //And increment everything to look for the next one
        priv->wr_buff_acq_idx++;
        if(unlikely(priv->wr_buff_acq_idx >= priv->wr_buffs_count)){ //But loop around
            priv->wr_buff_acq_idx = 0;
        }
    }

    if(likely(msg != NULL)){
        cbuff_unuse_front(priv->wr_buff_q);
    }

    if(likely(priv->wr_buff_q->in_use == 0)){
        return CAMIO_ETRYAGAIN;
    }

    //DBG("There are now %lli new write buffers available and %lli items in total\n", priv->wr_buff_q->in_use, priv->wr_buff_q->count);
    return CAMIO_ENOERROR;
}


camio_error_t bring_write_buffer_result(camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io)
{
    //DBG("Getting write buffer result\n");

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->wr_buff_q->in_use <= 0)){
        camio_error_t err = bring_write_buffer_ready(&this->wr_buff_muxable);
        if(unlikely(err)){
            DBG("There are no new buffers available to return, try again another time\n");
            return err;
        }
    }

    const ch_word count = MIN(*vec_len_io, priv->wr_buff_q->in_use);
    *vec_len_io = count;
    for(ch_word i = 0; i < count; i++, cbuff_pop_front(priv->wr_buff_q)){
        camio_msg_t* msg = cbuff_peek_front(priv->wr_buff_q);
        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }
        if(unlikely(msg->type != CAMIO_MSG_TYPE_WRITE_BUFF_RES)){
            ERR("Expected a write buffer result (%lli) but got %lli instead.\n",CAMIO_MSG_TYPE_WRITE_BUFF_RES, msg->type);
            continue;
        }

        res_vec[i] = *msg;
        const camio_wr_buff_res_t* res = &res_vec[i].wr_buff_res;

        if(likely(res->status == CAMIO_ENOERROR)){
            //DBG("Returning write buffer data_start=%p, data_size=%lli\n", res->buffer->data_start, res->buffer->data_len );
        }
        else{
            DBG("Error returning write buffer result. Err=%i\n", res->status);
        }
    }

    //DBG("Returning %lli write buffers and %lli left. There are %lli outstanding\n", count, priv->wr_buff_q->in_use, priv->wr_buff_q->count);
    return CAMIO_ENOERROR;

}



static camio_error_t bring_write_buffer_release(camio_channel_t* this, camio_buffer_t* buffer)
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
    buffer->data_len            = 0;
    buffer->data_start          = NULL;

    set_head(priv->rd_buffs,priv->wr_rel_index,BRING_SYNC_BUFF_EMPTY,buffer->data_len);

    priv->wr_rel_index++;
    if(unlikely(priv->wr_rel_index >= priv->wr_buffs_count)){ //But loop around
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
static camio_error_t bring_write_data_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io)
{

    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    //DBG("Doing write request -- there are currently %lli items in the queue\n", priv->wr_data_q->count);

    //Trim so that we fit
    ch_word vec_len = *vec_len_io;
    if(eqlikely(vec_len > priv->wr_data_q->size - priv->wr_data_q->count)){
        DBG("Triming vec len was=%lli", vec_len);
        vec_len = priv->wr_data_q->size - priv->wr_data_q->count;
        DBG2("now =%lli\n", vec_len);
    }

    //Try to perform the write requests
    ch_word sent = 0;
    for(ch_word i = 0 ; i < vec_len; i++){
         camio_msg_t* msg = &req_vec[i];

        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_WRITE_DATA_REQ)){
            ERR("Expected a write data request (%i) but got %i instead.\n",CAMIO_MSG_TYPE_WRITE_DATA_REQ, msg->type);
            continue;
        }

        camio_wr_data_req_t* req = &req_vec[i].wr_data_req;
        req_vec[i].type= CAMIO_MSG_TYPE_WRITE_DATA_RES;
        camio_buffer_t* buff = req->buffer;
        *vec_len_io = i;

//        DBG("Trying to process write data request of size %lli with data start=%p index=%lli\n",
//                req->buffer->data_len, req->buffer->data_start, req->buffer->__internal.__buffer_id);

        msg = cbuff_push_back(priv->wr_data_q,msg);
        if(unlikely(!msg)){
            ERR("Could not push item on to queue??");
            return CAMIO_EINVALID; //Exit now, this is unrecoverable
        }

        camio_wr_data_res_t* res = &msg->wr_data_res;
        msg->type = CAMIO_MSG_TYPE_WRITE_DATA_RES;

        if(unlikely(req->dst_offset_hint != CAMIO_WRITE_REQ_DST_OFFSET_NONE)){
            ERR("Could not complete request %i, DST_OFFSET must be NONE\n");
            res->status = CAMIO_EINVALID; //TODO XXX get a better error code
            continue;
        }
        if(unlikely(req->src_offset_hint != CAMIO_WRITE_REQ_SRC_OFFSET_NONE)){
            ERR("Could not complete request %i, SRC_OFFSET must be NONE\n");
            res->status = CAMIO_EINVALID; //TODO XXX get a better error code
            continue;
        }

        //Check that the buffers are being used in order
        if(unlikely(buff->__internal.__buffer_id != priv->wr_out_index)){
            ERR("Cannot use this buffer (id=%lli) until buffer with id=%lli is used\n",
                buff->__internal.__buffer_id,
                priv->wr_out_index
            );
            res->status = CAMIO_EWRONGBUFF;
            exit(1);
            continue;
        }

        //Check that the data is not corrupted
        if(unlikely(buff->data_len > buff->__internal.__mem_len)){
            ERR("Data length (%lli) is longer than buffer length (%lli), corruption has probably occured\n",
                buff->data_len,
                buff->__internal.__mem_len
            );
            res->status = CAMIO_ETOOMANY;
            return CAMIO_EINVALID; //Exit now, this is unrecoverable
        }

        //OK. Looks like the request is ok, do we have space for it, we should do!
        //Make the write visible to the read side
        set_head(priv->wr_buffs,priv->wr_out_index,priv->wr_sync_counter,buff->data_len);

//        DBG("Write data request to idx=%lli of size %lli with data start=%p sent with seq=%lli\n",
//                priv->wr_out_index, req->buffer->data_len, req->buffer->data_start, priv->wr_sync_counter);

        sent++;
        priv->wr_sync_counter++;
        priv->wr_out_index++;
        if(unlikely(priv->wr_out_index >= (ch_word)priv->wr_buffs_count)){
            priv->wr_out_index = 0;
        }
    }

    //DBG("There are %lli write datas sent to the receiver from a total of %lli\n", sent, priv->wr_data_q->count);
    return CAMIO_ENOERROR;
}



//Is the underlying channel done writing and ready for more?
static camio_error_t bring_write_data_ready(camio_muxable_t* this)
{
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);
    camio_msg_t* msg = cbuff_use_front(priv->wr_data_q);
    for(; msg != NULL; msg = cbuff_use_front(priv->wr_data_q)){

        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_WRITE_DATA_RES)){
            ERR("Expected a write data response (%lli) but got %lli instead.\n",CAMIO_MSG_TYPE_WRITE_DATA_RES, msg->type);
            continue;
        }

        camio_wr_data_res_t* res = &msg->wr_data_res;
        if(unlikely(res->status)){
            //An error was detected, so there's no point in going on with this request.
            continue;
        }

        const ch_word buff_idx = res->buffer->__internal.__buffer_id;
        volatile ch_word seq_no = get_head_seq(priv->wr_buffs, buff_idx);

        if(likely(seq_no != BRING_SYNC_BUFF_RXD)){
            break;
        }

        volatile ch_word data_size = get_head_size(priv->wr_buffs, buff_idx);
        //DBG("Buffer at index=%lli hass been read by the receiver. %lli bytes transfered!\n", buff_idx, data_size);
        res->written = data_size;

        //This is an auto release channel, once buffer is written out, it goes away, you'll need to acquire another
        camio_error_t err = camio_chan_wr_buff_release(this->parent.channel,res->buffer);
        if(unlikely(err)){
            return err;
        }

        //Tell the user that we released this buffer!
        res->buffer = NULL;
        res->status = CAMIO_EBUFFRELEASED;
    }

    if(likely(msg != NULL)){
        //DBG("Freeing unused request\n");
        cbuff_unuse_front(priv->wr_data_q);
    }

    if(likely(priv->wr_data_q->in_use == 0)){
        //DBG("There are no buffers yet available\n");
        return CAMIO_ETRYAGAIN;
    }

    //DBG("%lli write data requests have been RX'd. \n", priv->wr_data_q->in_use);
    return CAMIO_ENOERROR;

}


static camio_error_t bring_write_data_result(camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io)
{
   // DBG("Getting write buffer result\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->wr_data_q->in_use <= 0)){
        camio_error_t err = bring_write_buffer_ready(&this->wr_data_muxable);
        if(unlikely(err)){
            ERR("There are no new buffers available to return, try again another time\n");
            return err;
        }
    }

    const ch_word count = MIN(*vec_len_io, priv->wr_data_q->in_use);
    *vec_len_io = count;
    for(ch_word i = 0; i < count; i++, cbuff_pop_front(priv->wr_data_q)){
        camio_msg_t* msg = cbuff_peek_front(priv->wr_data_q);

        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }

        if(unlikely(msg->type != CAMIO_MSG_TYPE_WRITE_DATA_RES)){
            ERR("Expected a write data response (%lli) but got %lli instead.\n",CAMIO_MSG_TYPE_WRITE_DATA_RES, msg->type);
            continue;
        }

        res_vec[i] = *msg;
        const camio_wr_data_res_t* res = &res_vec[i].wr_data_res;

        if(unlikely(res->status == CAMIO_ENOERROR)){
            //DBG("Returning write result data_start=%p, data_size=%lli\n", res->buffer->data_start, res->buffer->data_len );
        }
        else if(likely(res->status == CAMIO_EBUFFRELEASED)){
            //DBG("Returning write result [Buffer released!]\n" );
        }
        else{
            DBG("Error returning write result. Err=%i\n", res->status);
        }
    }

    //DBG("Returning %lli write data completion messages\n", count);
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

    if(this->rd_data_muxable.fd > -1){
        close(this->rd_data_muxable.fd);
        this->rd_data_muxable.fd = -1; //Make this reentrant safe
        this->wr_data_muxable.fd = -1; //Make this reentrant safe
    }

    if(priv->rd_data_q){
        cbuff_delete(priv->rd_data_q);
        priv->rd_data_q = NULL;
    }

    if(priv->wr_data_q){
        cbuff_delete(priv->wr_data_q);
        priv->wr_data_q = NULL;
    }

    if(priv->wr_buff_q){
        cbuff_delete(priv->wr_buff_q);
        priv->wr_buff_q = NULL;
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

    //DBG("Constructing bring channel\n");
    bring_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    priv->controller  = *controller; //Keep a copy of the controller state
    priv->params     = *params;
    priv->bring_head = bring_head;

    //Connect the read and write ends together
    priv->rd_mem            = (char*)bring_head + bring_head->rd_mem_start_offset;
    priv->rd_buffs_count  = bring_head->rd_slots;
    priv->rd_buffs        = (camio_buffer_t*)calloc(bring_head->rd_slots,sizeof(camio_buffer_t));
    if(!priv->rd_buffs){
        ERR("Ran out of memory allocating buffer pool\n");
        return CAMIO_ENOMEM;
    }
    for(int i = 0; i < bring_head->rd_slots;i++){
        priv->rd_buffs[i].__internal.__buffer_id       = i;
        priv->rd_buffs[i].__internal.__do_auto_release = false;
        priv->rd_buffs[i].__internal.__in_use          = false;
        priv->rd_buffs[i].__internal.__mem_start       = (char*)priv->rd_mem + bring_head->rd_slots_size * i;
        priv->rd_buffs[i].__internal.__mem_len         = bring_head->rd_slots_size - sizeof(bring_slot_header_t);
        priv->rd_buffs[i].__internal.__parent          = this;
        priv->rd_buffs[i].__internal.__pool_id         = 0;
        priv->rd_buffs[i].__internal.__read_only       = true;
    }

    priv->wr_mem            = (char*)bring_head + bring_head->wr_mem_start_offset;
    priv->wr_buffs_count  = bring_head->wr_slots;
    priv->wr_buffs        = (camio_buffer_t*)calloc(bring_head->wr_slots,sizeof(camio_buffer_t));
    if(!priv->wr_buffs){
        free(priv->rd_buffs);
        ERR("Ran out of memory allocating buffer pool\n");
        return CAMIO_ENOMEM;
    }
    for(int i = 0; i < bring_head->wr_slots;i++){
        priv->wr_buffs[i].__internal.__buffer_id       = i;
        priv->wr_buffs[i].__internal.__do_auto_release = true;
        priv->wr_buffs[i].__internal.__in_use          = false;
        priv->wr_buffs[i].__internal.__mem_start       = (char*)priv->wr_mem + bring_head->wr_slots_size * i;
        priv->wr_buffs[i].__internal.__mem_len         = bring_head->wr_slots_size - - sizeof(bring_slot_header_t);
        priv->wr_buffs[i].__internal.__parent          = this;
        priv->wr_buffs[i].__internal.__pool_id         = 0;
        priv->wr_buffs[i].__internal.__read_only       = true;
    }

    if(!priv->params.server){ //Swap things around to connect both ends
        ch_word tmp_buffers_count    = priv->rd_buffs_count;
        camio_buffer_t* tmp_buffers  = priv->rd_buffs;

        priv->wr_mem                 = priv->rd_mem;
        priv->wr_buffs_count       = priv->rd_buffs_count;
        priv->wr_buffs             = priv->rd_buffs;

        priv->rd_buffs_count       = tmp_buffers_count;
        priv->rd_buffs             = tmp_buffers;
    }

    priv->rd_sync_counter   = 2; //This is where we expect the first counter
    priv->wr_sync_counter   = 2;


    //TODO: Should wire in this parameter from the outside, 1024 will do for the moment.
    priv->rd_buff_q     = ch_cbuff_new(priv->rd_buffs_count,sizeof(camio_msg_t));
    priv->rd_data_q     = ch_cbuff_new(priv->rd_buffs_count,sizeof(camio_msg_t));
    priv->wr_buff_q     = ch_cbuff_new(priv->wr_buffs_count,sizeof(camio_msg_t));
    priv->wr_data_q     = ch_cbuff_new(priv->wr_buffs_count,sizeof(camio_msg_t));

    (void)fd;
    //DBG("Done constructing BRING channel with fd=%i\n", fd);
    return CAMIO_ENOERROR;

}


NEW_CHANNEL_DEFINE(bring,bring_channel_priv_t)

