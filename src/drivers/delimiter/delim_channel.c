/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   26 Jun 2015
 *  File name: delim_channel.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include <src/devices/channel.h>
#include <src/devices/controller.h>
#include <src/buffers/buffer_malloc_linear.h>
#include <deps/chaste/utils/util.h>
#include <src/api/api.h>

#include <deps/chaste/utils/debug.h>

#include "delim_channel.h"

#include <fcntl.h>
#include <errno.h>

/**************************************************************************************************************************
 * PER CHANNEL STATE
 **************************************************************************************************************************/
#define BASE_MSGS_MAX 256
typedef enum {
    DELIM_STATE_WB_NEED_MORE = 0,
    DELIM_STATE_WORK_BUFF,
    DELIM_STATE_RDY_RD_BUFFS,
    DELIM_STATE_RDY_RD_DATAS,
} delim_states_e;


typedef struct delim_channel_priv_s {
    camio_channel_t* base; //Base channel that will be "decorated" by this delimiter

    //The working buffer is where we do stuff until there is some kind of output ready
    camio_buffer_t rd_working_buff;

    //The result buffers is where we put stuff when there are results to send out to the outside world
    ch_word rd_buffs_count;     //Number of result buffers and request queue slots
    ch_cbq_t* rd_buff_q;        //queue for inbound read buffer requests
    ch_cbq_t* rd_data_q;      //queue for inbound read data requests
    camio_buffer_t* rd_buffs;   //All buffers for the read side, basically pointers to the working buffer
    ch_word rd_acq_index;       //Current index for acquiring new read buffers
    ch_word rd_rel_index;       //Current index for releasing new read buffers

    camio_msg_t* base_msgs;
    ch_word base_msgs_len;
    ch_word base_msgs_max;

    delim_states_e state; //We use a state machine to make progress through reading and working on the underlying channel
    ch_word resuse_buffers;
    ch_word unused_rd_buff_resps;
    ch_word readable_buffs;

    //The base variables are used to gather data from the underlying channel
//    camio_buffer_t* rd_base_buff;
//    camio_rd_req_t* rd_base_req_vec;
//    ch_word rd_base_req_vec_len;
//    ch_bool rd_base_registered;

    ch_bool is_rd_closed;
//    ch_bool read_registered;

    ch_word (*delim_fn)(char* buffer, ch_word len);

} delim_channel_priv_t;



/**************************************************************************************************************************
 * READ FUNCTIONS - READ BUFFER REQUEST
 **************************************************************************************************************************/
static void delim_read_close(camio_channel_t* this){
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    if(!priv->is_rd_closed){
        free(priv->rd_working_buff.__internal.__mem_start);
        reset_buffer(&priv->rd_working_buff);
        //reset_buffer(&priv->rd_result_buff);
        priv->is_rd_closed = true;
    }
}


static camio_error_t delim_read_buffer_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io )
{
    DBG("Doing delim read buffer request...!\n");
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    if(unlikely(NULL == cbuff_push_back_carray(priv->rd_buff_q, req_vec, vec_len_io))){
        ERR("Could not push any items on to queue.");
        return CAMIO_EINVALID;
    }

    DBG("delim read buffer request done - %lli requests added\n", *vec_len_io);
    return CAMIO_ENOERROR;
}




static inline camio_error_t grow_working_buff(delim_channel_priv_t* priv) {
    if(!priv->rd_base_buff){
        return CAMIO_ENOERROR; //No need to grow buffer because no new space is needed
    }


    //     ( The amount of free space in the buffer)                                     <  (Extra space needed         )
    while ((priv->rd_working_buff.__internal.__mem_len - priv->rd_working_buff.data_len) < (priv->rd_base_buff->data_len)) {
        ERR("Growing working buffer from %lu to %lu\n", priv->rd_working_buff.__internal.__mem_len,
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

static camio_error_t req_read_data(camio_channel_t* this)
{
    DBG("Requesting ready data\n");
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    camio_error_t err = CAMIO_ENOERROR;

    //So, we either have or need to get some more buffer responses. Figure out which
    if(eqlikely(priv->resuse_buffers)){ //We can reuse buffers!
        DBG("There are %lli reusable, buffer responses we're going to try and use!\n", priv->resuse_buffers);
        priv->base_msgs_len = priv->resuse_buffers;
    }
    else if(eqlikely(priv->unused_rd_buff_resps)){ //We have some left over buffer responses
        DBG("There are %lli unused, buffer responses we're going to try and use!\n", priv->unused_rd_buff_resps);
        priv->base_msgs_len = priv->unused_rd_buff_resps;
    }
    else{ //We need some more buffers
        DBG("Waiting to see if there are some new read buffers ready\n");
        //Try to see if the underlying is ready, if not, come back later
        err = camio_chan_rd_buff_ready(priv->base);
        if(likely(err)){//Try again sucker! - expect this to happen a lot
            return err;
        }
        DBG("There are new read buffers ready\n");

        //The underlying is ready with some buffers, grab them
        priv->base_msgs_len = priv->base_msgs_max;
        DBG("Getting at most %lli read_buffs\n", priv->base_msgs_len);
        err = camio_chan_rd_buff_res(priv->base,priv->base_msgs,&priv->base_msgs_len);
        if(unlikely(err)){
            DBG("Don't know what to do next!\n");
            priv->state = DELIM_STATE_WORK_BUFF; //Not sure what the right thing to do here is? Just reset??
            return err;
        }
        DBG("There are new %lli read buffers available\n", priv->base_msgs_len);
    }

    //OK! We have now have some buffers, let's request the data to fill them
    ch_word rd_buffs = priv->base_msgs_len;
    err = camio_chan_rd_data_req(priv->base,priv->base_msgs + priv->unused_rd_buff_resps, &priv->base_msgs_len);
    if(err){
        DBG("Don't know what to do next!\n");
        priv->state = DELIM_STATE_WORK_BUFF; //Not sure what the right thing to do here is? Just reset??
        return err;
    }

    //Did all of the requests make it in?? If not, we will have some left over unused_rd_buffer responses
    priv->unused_rd_buff_resps = priv->base_msgs_len - rd_buffs;
    priv->readable_buffs = priv->base_msgs_len;
    DBG("Have now issued requested %lli data requests from %lli buffers\n", priv->base_msgs_len, rd_buffs);
    return err;
}


static camio_error_t get_new_buffers(camio_channel_t* this)
{
    //We need some more data, try to get some, but only do so if we don't have some outstanding requests or buffers
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    if(eqlikely(priv->unused_rd_buff_resps || priv->resuse_buffers)){
        DBG("We don't need more buffers\n");
        return CAMIO_ENOERROR;
    }

    //Initialize a batch of messages to request some write buffers
    for(int i = 0; i < priv->base_msgs_max; i++){
        priv->base_msgs[i].type = CAMIO_MSG_TYPE_READ_BUFF_REQ;
        priv->base_msgs[i].id = i;
        camio_rd_buff_req_t* req = &priv->base_msgs[i].rd_buff_req;
        req->dst_offset_hint = CAMIO_READ_REQ_DST_OFFSET_NONE;
        req->src_offset_hint = CAMIO_READ_REQ_SRC_OFFSET_NONE;
        req->read_size_hint  = CAMIO_READ_REQ_SIZE_ANY;
    }

    priv->base_msgs_len = priv->base_msgs_max;
    DBG("Requesting %lli read_buffs\n", priv->base_msgs_len);
    camio_error_t err = camio_chan_rd_buff_req( priv->base, priv->base_msgs, &priv->base_msgs_len);
    if(err){
        DBG("Could not request buffers with error %lli\n", err);
        return err;
    }
    DBG("Successfully issued %lli buffer requests\n", priv->base_msgs_len);
    return CAMIO_ENOERROR;
}



static camio_error_t delim_read_peek_basic(camio_channel_t* this, camio_buffer_t* result)
{
    DBG("Doing read peek\n");
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //First, try the easy approach, if there is data in the working buffer, try the delimiter function to see...
    if(priv->rd_working_buff.data_len > 0){
        const ch_word delimit_size = priv->delim_fn(priv->rd_working_buff.data_start, priv->rd_working_buff.data_len);
        if(delimit_size > priv->rd_working_buff.data_len){
            ERR("your delimiter is buggy, you cannot return more data  (%lli) than we gave you (%lli)!\n",
                    delimit_size, priv->rd_working_buff.data_len);
            return CAMIO_ETOOMANY;
        }

        if(delimit_size > 0){
            //OK! Looks like we have a good delimit!
            buffer_slice(result, &priv->rd_working_buff,priv->rd_working_buff.data_start, delimit_size );
            DBG("Yay! Delimiter first time success on workig buffer with data of size %lli!\n", delimit_size);
            return CAMIO_ENOERROR;
        }
    }
    return CAMIO_ETRYAGAIN;
}



//See if there is something to read
static camio_error_t delim_read_peek( camio_channel_t* this, camio_buffer_t* result)
{
    DBG("Doing read peek\n");
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);


    camio_error_t err = CAMIO_ENOERROR;
    switch(priv->state){
        case DELIM_STATE_WORK_BUFF:{ //The working buffer is not empty
            DBG("There is data in the working buffer, we're doing to try to delimit it\n");
            err = delim_read_peek_basic(this,result);
            if(eqlikely(err != CAMIO_ETRYAGAIN)){ return err; }
            priv->state = DELIM_STATE_WB_NEED_MORE;
        }
        // no break; -- fall through is intentional
        case DELIM_STATE_WB_NEED_MORE:{ //The working buffer needs more data
            err = get_new_buffers(this);
            if(unlikely(err)){ return err; }
            priv->state = DELIM_STATE_RDY_RD_BUFFS;
        }
        // no break; -- fall through is intentional
        case DELIM_STATE_RDY_RD_BUFFS:{
            DBG("Checking for read buffers\n");
            err = req_read_data(this);
            if(likely(err)){ return err; } //Expect this to happy a lot thanks to the ready function
            priv->state = DELIM_STATE_RDY_RD_DATAS;
        }
        // no break; -- fall through is intentional
        case DELIM_STATE_RDY_RD_DATAS:{
            //Try to see if the underlying is ready, if not, come back later
            err = camio_chan_rd_data_ready(priv->base);
            if(likely(err)) { return err; } //Try again sucker! - expect this to happen a lot

            DBG("There are new read datas available\n");
            //The underlying says there is some new data for us! Yay!
            if(priv->resuse_buffers)
            priv->base_msgs_len = priv->base_msgs_max;
            err = camio_chan_rd_data_res(priv->base,priv->base_msgs,&priv->base_msgs_len);
            if(err){
                DBG("Don't know what to do next!\n");
                priv->state = DELIM_STATE_WORK_BUFF; //Not sure what the right thing to do here is? Just reset??
                return err;
            }

            DBG("There are new %lli read datas available\n", priv->base_msgs_len);


            priv->state = DELIM_STATE_RDY_RD_BUFFS;
            break;
        }
    }



    //Damn, OK. That did't work. Try to get some more data from the base channel
    //First get some buffers -- It doesn't matter if we did this last time, we may as well keep the pipe full for next time
    camio_error_t err = get_new_buffers();
    if(err && err != CAMIO_ETOOMANY){ //ETOOMANY is ok, is just means the base channel is currently full with buffer requests
        DBG("Could not get some more buffers\n");
        return err;
    }


    //Now, wait until some of those buffers become available
    err = camio_chan_rd_buff_ready(priv->base);
    if(err && err != CAMIO_ETRYAGAIN){ //We can ignore ETRYAGAIN on the hope the
        DBG("Error in ready function on getting base buffers\n");
    }

    //Now that a read has been registered, see if there is any data available
    camio_error_t err = camio_read_ready(priv->base);
    if(err == CAMIO_ENOTREADY){
        return CAMIO_ETRYAGAIN;
    }
    if( err != CAMIO_EREADY){
        ERR("Unexpected error from base ready()!\n");
        return err; //Damn, no data available. Come back later and try again.
    }
    DBG("Data is ready to be read from base!\n");

    //Yes! We have some more data. Get at it.
    err = camio_read_acquire(priv->base,&priv->rd_base_buff);
    if(err){
        ERR("Could not do read acquire on base channel\n");
        return err;
    }
    DBG("Read another %lli bytes from %p. Data starts at %p\n", priv->rd_base_buff->data_len, priv->rd_base_buff, priv->rd_base_buff->data_start);
    priv->rd_base_registered = false; //We got the data, next time we will need to register again.

    //Got some data, but is it just a closed channel?
    if(priv->rd_base_buff->data_len == 0){
        //There is no more data to read, time to give up
        delim_read_close(this);
        DBG("Base channel is closed, closing delimiter!\n");
        return CAMIO_ENOERROR;
    }

    // ---- Now the real work begins. We have new data. So can we delimit it?

    //We have real data and the working buffer is empty, try a fast path. Maybe we won't need to copy into the working buffer
    if(priv->rd_working_buff.data_len == 0){
        DBG("Trying empty working buffer fast path...\n");
        DBG("Priv is %p\n", priv);
        DBG("priv=%p Dlimiter fn=%p\n", priv, priv->delim_fn);
        const ch_word delimit_size = priv->delim_fn(priv->rd_base_buff->data_start, priv->rd_base_buff->data_len);
        DBG("size=%lli\n", delimit_size);
        if(delimit_size > 0) { //Success! There is a valid packet in the read buffer alone.
            DBG("Fast path match from delimiter of size %lli!\n", delimit_size);
            buffer_slice(&priv->rd_result_buff,priv->rd_base_buff,priv->rd_base_buff->data_start, delimit_size);
            return CAMIO_ENOERROR;
        }
        DBG("Datagram fast path fail. Try slow path...\n");
    }

    //Just check that this is assumption true. If it's not, the following code will break!
    if(priv->rd_working_buff.data_start != priv->rd_working_buff.__internal.__mem_start){
        ERR("Invariant violation. Assumptions will be broken!\n");
        return CAMIO_EINVALID;
    }

    //Let's put the data somewhere that we can work with it later. First make sure we have enough space:
    err = grow_working_buff(priv);
    if (err){
        ERR("Could not grow working buff\n");
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


static camio_error_t delim_read_buffer_ready(camio_muxable_t* this)
{
    DBG("Checking if delimiter is ready\n");
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Try to fill as many read requests as we can
    camio_msg_t* msg = cbuff_use_front(priv->rd_buff_q);
    for(; msg != NULL; msg = cbuff_use_front(priv->rd_buff_q)){
        //Sanity check the message first
        if(unlikely(msg->type == CAMIO_MSG_TYPE_IGNORE)){
            continue; //We don't care about this message
        }
        if(unlikely(msg->type != CAMIO_MSG_TYPE_READ_BUFF_REQ)){
            ERR("Expected a read buffer request message (%i) but got %i instead.\n",
                    CAMIO_MSG_TYPE_READ_BUFF_REQ, msg->type);
            continue;
        }

        //Extract the request pointer and check it as well.
        camio_rd_buff_req_t* req = &msg->rd_buff_req;
        camio_rd_buff_res_t* res = &msg->rd_buff_res;
        if(unlikely(req->dst_offset_hint != CAMIO_READ_REQ_DST_OFFSET_NONE)){
            ERR("Could not process request %lli, DST_OFFSET must be NONE\n");
            msg->type = CAMIO_MSG_TYPE_READ_BUFF_RES;
            res->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }
        if(unlikely(req->src_offset_hint != CAMIO_READ_REQ_SRC_OFFSET_NONE)){
            ERR("Could not process request %lli, SRC_OFFSET must be NONE\n");
            msg->type = CAMIO_MSG_TYPE_READ_BUFF_RES;
            res->status = CAMIO_EINVALID; //TODO better error code here!
            continue;
        }

        //OK, see if we can get a new result
        camio_buffer_t* result = priv->rd_buffs[priv->rd_acq_index];
        camio_error_t err = delim_read_peek(this->parent.channel, result);
        if(err){
            DBG("Delimiter reports that there is nothing new to consume\n");
            break;
        }

        //WOOT! New data ready to send to user
        msg->type = CAMIO_MSG_TYPE_READ_BUFF_RES;
        res->status = CAMIO_ECANNOTREUSE; //TDOD XXX: Actually this might be possible depending on the base channel
        res->buffer = result;

        priv->rd_acq_index++;   //Move to the next index in buffer acquisition
        if(unlikely(priv->rd_acq_index >= priv->rd_buffs_count)){ //But loop around
            priv->rd_acq_index = 0;
        }

    }

    if(likely(msg != NULL)){
        cbuff_unuse_front(priv->rd_buff_q);
    }

    if(unlikely(priv->rd_buff_q->in_use >= priv->rd_buffs_count)){
        DBG("Cannot try to produce any more results until you release some buffers\n");
        return CAMIO_ENOBUFFS;
    }
    if(likely(priv->rd_buff_q->in_use == 0)){
        return CAMIO_ETRYAGAIN;
    }

    //DBG("There are %lli new buffers to read into\n", priv->rd_buff_q->in_use);
    return CAMIO_ENOERROR;
}


static camio_error_t delim_read_buffer_result( camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io )
{
    DBG("Acquiring buffer\n");

    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        ERR("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( NULL == buffer_o){
        ERR("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }
    if( NULL != *buffer_o){
        ERR("Buffer chain not null. You should release this before getting a new one, otherwise dangling pointers!\n");
        return CAMIO_EINVALID;
    }
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
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
 *          (a) The delimiter was successful because the working buffer was empty and the base channel returned a result that
 *              is the exact size the delimiter was expecting. In this case, the delimiter is basically "pass-through"
 *              and the base channel then needs read_release to be called.
 *          (b) The delimiter was successful because it found a match in the working buffer. In this case, we need to resize
 *              and potentially move data.
 *      (2) We have optimistically found another delimited result in a previous call to read_release().  In this case
 *          priv->work_rd_buff.data_len may be less than priv->result_rd_buff.data_size. Again, we may need to resize and
 *          move data.
 *
 *  We must handle both of these cases in this function
 */
static camio_error_t delim_read_buffer_release(camio_channel_t* this, camio_buffer_t* buffer)
{
    DBG("Releasing read\n");

    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        ERR("This null???\n"); //WTF?
        return CAMIO_EINVALID;
    }


    if( NULL == buffer){
        ERR("Buffer chain pointer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    if( NULL == *buffer){
        ERR("Buffer null\n"); //WTF?
        return CAMIO_EINVALID;
    }

    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    if(!priv->read_registered){
        ERR("WFT? Why are you trying to release something if you haven't read anyhting?\n");
        return CAMIO_EINVALID;
    }

    //Deal with the basic release operations.
    priv->read_registered = false;
    *buffer = NULL; //Remove dangling pointer for the source

     //We arrived here because for one of two reasons
    //  1) we read successfully delimited from the base channel. So we're still hanging on to the base channel buffer
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
        DBG("There's no data left. We're done for now!\n");
        if(priv->rd_base_buff){
            DBG("Releasing underlying fast_path buffer\n");
            //The result is directly from the underlying channel, so we should release it now.
            camio_read_release(priv->base,&priv->rd_base_buff);
        }
        priv->rd_working_buff.data_start = NULL;
        priv->rd_working_buff.data_len   = 0;
        goto reset_and_exit;
    }

    //OK. so there's some data hanging around, let's try to delimit it

    const ch_word data_next_len = data_remain;

    const ch_word delimit_size = priv->delim_fn(data_next, data_next_len);
    if(delimit_size > 0 && delimit_size <= data_next_len){
        //Ready for the next round with data available! No memmove required!
        DBG("Success! Delimt size = %lli, No memmove required this time!\n", delimit_size);
        priv->rd_result_buff.data_start = data_next;
        priv->rd_result_buff.data_len   = delimit_size;
        return CAMIO_ENOERROR;
    }
    DBG("Failed to delimit, will have to move memory\n");

    //Nope. No success with the delimiter. Looks like we have to move stuff around. Move waht we have right back to the
    //beginning of the working buffer. First make sure that the buffer is big enough
    camio_error_t err = grow_working_buff(priv);
    if (err){
        ERR("Could not grow working buff\n");
        return err;
    }

//    ERR("Doing mem move of %lli from %p to %p\n", data_next_len, data_next,
//            priv->rd_working_buff.__internal.__mem_start );
    memmove(priv->rd_working_buff.__internal.__mem_start, data_next, data_next_len);
    priv->rd_working_buff.data_start = priv->rd_working_buff.__internal.__mem_start;
    priv->rd_working_buff.data_len   = data_next_len;

    //If we still have a base buffer at this point, we should get rid of it
    if(priv->rd_base_buff){
        DBG("Releasing underlying fast_path buffer\n");
        //The result is directly from the underlying channel, so we should release it now.
        camio_read_release(priv->base,&priv->rd_base_buff);
    }

reset_and_exit:
    DBG("Releasing result buffer and exiting\n");
    reset_buffer(&priv->rd_result_buff); //We're done with this buffer pointer for the moment until there's new data

    return CAMIO_ENOERROR;
}

/**************************************************************************************************************************
 * READ FUNCTIONS - READ DATA REQUESTS
 **************************************************************************************************************************/

static camio_error_t delim_read_data_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io )
{
    DBG("Doing delim read request...!\n");
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    if(unlikely(NULL == cbuff_push_back_carray(priv->rd_data_q, req_vec,vec_len_io))){
        ERR("Could not push any items on to queue.");
        return CAMIO_EINVALID;
    }

    DBG("delim read data request done - %lli requests added\n", *vec_len_io);
    return CAMIO_ENOERROR;
}


//When we enter this function, we can assume that we have already acquired the data buffer in the a call to
//read_buffer_request. This means that all we need to do is check that the buffer that came in, belonged to us. If so, simply
//forward the buffer back out to the user. This complicated little dance is necessary to make other transports work.
//In theory, we could make this function actually take buffers from the outside world by acquiring an internal buffer and
//copying. There is a bit of dancing required to get around the async calls so I'll defer this work to a TODO XXX.
static camio_error_t delim_read_data_ready(camio_muxable_t* this)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this->parent.channel);
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
            ERR("Cannot use a NULL buffer. You need to call read_buffer_request first?\n");
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


static camio_error_t delim_read_data_result( camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io )
{
    //DBG("Trying to get read result\n");
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->rd_data_q->in_use <= 0)){
        camio_error_t err = delim_read_data_ready(&this->rd_data_muxable);
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
 * WRITE FUNCTIONS -- These just all pass through to the underlying
 **************************************************************************************************************************/

static camio_error_t delim_write_buffer_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    return camio_chan_wr_buff_req(priv->base,req_vec,vec_len_io);
}

static camio_error_t delim_write_buffer_ready(camio_muxable_t* this)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    return camio_chan_wr_buff_ready(priv->base);
}

static inline camio_error_t delim_write_buffer_result(camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    return camio_chan_wr_buff_res(priv->base,res_vec,vec_len_io);
}

static camio_error_t delim_write_buffer_release(camio_channel_t* this, camio_buffer_t* buffer)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    return camio_chan_wr_buff_release(priv->base, buffer);
}

static camio_error_t delim_write_data_request(camio_channel_t* this, camio_msg_t* req_vec, ch_word* vec_len_io)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    return camio_chan_wr_data_req(priv->base,req_vec,vec_len_io);
}

static camio_error_t delim_write_data_ready(camio_muxable_t* this)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    return camio_chan_wr_data_ready(priv->base);
}

static inline camio_error_t delim_write_data_result(camio_channel_t* this, camio_msg_t* res_vec, ch_word* vec_len_io)
{
    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    return camio_chan_wr_data_res(priv->base,res_vec,vec_len_io);
}

/**************************************************************************************************************************
 * SETUP/CLEANUP FUNCTIONS
 **************************************************************************************************************************/

static void delim_destroy(camio_channel_t* this)
{
    //Basic sanity checks -- TODO DELIM: Should these be made into (compile time optional?) asserts for runtime performance?
    if( NULL == this){
        ERR("This null???\n"); //WTF?
        return;
    }
    delim_read_close(this);

    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);
    if(priv->rd_working_buff.__internal.__mem_start){
        free(priv->rd_working_buff.__internal.__mem_start);
    }

    camio_channel_destroy(priv->base);

    free(this);

}

#define DELIM_BUFFER_DEFAULT_SIZE (64 * 1024) //64KB
camio_error_t delim_channel_construct(
    camio_channel_t* this,
    camio_controller_t* controller,
    camio_channel_t* base_channel,
    ch_word(*delim_fn)(char* buffer, ch_word len),
    ch_word rd_buffs_count
)
{
    (void)controller; //Nothing to do with this

    delim_channel_priv_t* priv = CHANNEL_GET_PRIVATE(this);

    priv->delim_fn  = delim_fn;
    priv->base      = base_channel;

    this->rd_data_muxable.fd                = base_channel->rd_data_muxable.fd;
    this->wr_data_muxable.fd                = base_channel->wr_data_muxable.fd;

    priv->rd_working_buff.__internal.__mem_start = calloc(1,DELIM_BUFFER_DEFAULT_SIZE);
    priv->rd_working_buff.__internal.__mem_len   = DELIM_BUFFER_DEFAULT_SIZE;
    priv->rd_working_buff.data_start   = priv->rd_working_buff.__internal.__mem_start;
    if(!priv->rd_working_buff.__internal.__mem_start){
        ERR("Could not allocate memory for backing buffer\n");
        return CAMIO_ENOMEM;
    }

    priv->rd_buffs_count = rd_buffs_count;
    priv->rd_buffs       = (camio_buffer_t*)calloc(rd_buffs_count,sizeof(camio_buffer_t));
    if(!priv->rd_buffs){
        DBG("Could not allocate memory for internal buffers\n");
        return CAMIO_ENOMEM;
    }
    for(ch_word i = 0; i < priv->rd_buffs_count; i++){
        priv->rd_buffs[i].__internal.__buffer_id       = i;
        priv->rd_buffs[i].__internal.__do_auto_release = false;
        priv->rd_buffs[i].__internal.__in_use          = false;
        priv->rd_buffs[i].__internal.__mem_start       = NULL;
        priv->rd_buffs[i].__internal.__mem_len         = 0;
        priv->rd_buffs[i].__internal.__parent          = this;
        priv->rd_buffs[i].__internal.__pool_id         = 0;
    }

    priv->rd_buff_q      = ch_cbuff_new(priv->rd_buffs_count,sizeof(camio_msg_t));
    if(!priv->rd_buff_q){
        DBG("Could not allocate memory for read buffers queue\n");
        return CAMIO_ENOMEM;
    }
    priv->rd_data_q      = ch_cbuff_new(priv->rd_buffs_count,sizeof(camio_msg_t));
    if(!priv->rd_data_q){
        DBG("Could not allocate memory for read data queue\n");
        return CAMIO_ENOMEM;
    }

    //TODO XXX - This should be populated as parameter, but not sure how to do it well. Don't want to as the user a question
    //they don't understand how to answer.
    priv->base_msgs_max = BASE_MSGS_MAX;
    priv->base_msgs = (camio_msg_t*)calloc(priv->base_msgs_max, sizeof(camio_msg_t));
    if(!priv->base_msgs){
        DBG("Could not allocate memory for base messages!\n");
        return CAMIO_ENOMEM;
    }

    DBG("Done constructing delim channel\n");
    return CAMIO_ENOERROR;
}


NEW_CHANNEL_DEFINE(delim,delim_channel_priv_t)
