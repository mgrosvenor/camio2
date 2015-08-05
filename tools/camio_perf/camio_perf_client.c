/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 25, 2015
 *  File name:  camio_perf_client.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include <stdio.h>

#include <deps/chaste/chaste.h>

#include <src/api/api_easy.h>
//#include <src/drivers/delimiter/delim_device.h>
#include "camio_perf_packet.h"
#include <src/devices/messages.h>

#include "options.h"
#include <deps/chaste/perf/perf.h>
extern struct options_t options;

//This is a single threaded app for the moment, so we'll just have one mux
static camio_mux_t* mux            = NULL;

//Device states
#define DEVS_MAX 1 //Only deal with one device at a time for the moment
//static camio_dev_t* dev_states[DEVS_MAX]  = {{0}};
//static ch_word num_devs                   = 0;
static camio_dev_t* delim                   = NULL;

//Per channel states
#define BUFFS_MAX 1024
#define CHANS_MAX 16
typedef struct perfc_chan_state_s {
    camio_msg_t state;
    camio_msg_t buff_states[BUFFS_MAX];
//    ch_word num_buffs;
} perf_chan_state_t;

static perf_chan_state_t chan_states[CHANS_MAX] = {{{0}}};
//static ch_word num_chans                        = 0;


static char* packet_data = NULL; //Placeholder for data so that we only copy rather than create each msg

#define MSGS_MAX 1024
static camio_msg_t data_msgs[MSGS_MAX] = {{0}};
static ch_word data_msgs_len           = MSGS_MAX;
static camio_msg_t chan_msgs[MSGS_MAX] = {{0}};
static ch_word chan_msgs_len           = MSGS_MAX;


static ch_bool started            = false;

//Statistics keeping
static ch_word time_now_ns  = 0;
static ch_word seq          = 0;
static ch_word intv_bytes   = 0;
static ch_word intv_samples = 0;

static camio_error_t get_new_buffers(camio_channel_t* channel, ch_word chan_id);
static camio_error_t send_perf_messages(camio_channel_t* channel, ch_word chan_id);
static camio_error_t get_new_channels();

//static ch_word delimit(char* buffer, ch_word len)
//{
//    if((size_t)len < sizeof(camio_perf_packet_head_t)){
//        return -1;
//    }
//
//    camio_perf_packet_head_t* head = (camio_perf_packet_head_t*)buffer;
//    if(len < head->size){
//        return -1;
//    }
//
//    return head->size;
//}

static camio_error_t connect_delim(ch_cstr client_channel_uri, camio_dev_t** device) {

//    //Find the ID of the delim channel
//    ch_word id;
//    camio_error_t err = camio_device_get_id("delim", &id);
//
//    //Fill in the delim channel paramters structure
//    delim_params_t delim_params = {
//            .base_uri = client_channel_uri,
//            .delim_fn = delimit
//    };
//    ch_word params_size = sizeof(delim_params_t);
//    void* params = &delim_params;
//
//    //Use the parameters structure to construct a new device object
//    err = camio_device_constr(id, &params, params_size, device);
//    if (err) {
//        DBG("Could not construct device\n");
//        return CAMIO_EINVALID; //TODO XXX put a better error here
//    }
//
//    //And we're done!
//    DBG("Got device\n");


    camio_easy_device_new(client_channel_uri,device);

    return CAMIO_ENOERROR;
}


static camio_error_t on_new_wr_datas(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Handling write complete on id=%lli\n", id);
    ch_perf_event_start(5,0,0);

    //Currently ignoring this values
    (void)usr_state;

    if(unlikely(err)){
        DBG("Unexpected error %lli\n", err);
        ch_perf_event_stop(5,0,0);
        return err;
    }

    data_msgs_len = MIN(MSGS_MAX,options.batching);
    err = camio_chan_wr_data_res(muxable->parent.channel, data_msgs, &data_msgs_len );
    if(unlikely(err)){
        DBG("Could not retrieve write data responses\n");
        ch_perf_event_stop(5,1,0);
        return err;
    }

    if(unlikely(data_msgs_len == 0)){
        DBG("Huh? Got no new write completions?\n");
        return CAMIO_EINVALID;
    }

        

    ch_perf_event_stop(51,0,0);
    
    ch_word reusable_buffs = 0; //See how many buffers we can potentially reuse
    ch_word batch_bytes = 0;    //How many bytes did send get in this batch?
    camio_msg_t* buff_states = chan_states[id].buff_states;
    for(ch_word i = 0 ; i < data_msgs_len; i++ ){
        const ch_word buff_id = data_msgs[i].id;
        buff_states[buff_id] = data_msgs[i];

        if(unlikely(data_msgs[i].type != CAMIO_MSG_TYPE_WRITE_DATA_RES)){
            WARN("Expected to get buffer  (id=%lli) response message (%lli), but got %i instead.\n",
                    data_msgs[i].id, CAMIO_MSG_TYPE_WRITE_DATA_RES, data_msgs[i].type);
            continue;
        }

        camio_wr_data_res_t* res = &data_msgs[i].wr_data_res;
        if(unlikely(res->status && res->status != CAMIO_EBUFFRELEASED)){
            ERR("Error %i getting data response with id=%lli\n", res->status, data_msgs[i].id);
            continue;
        }

        //DBG("Sent %lli bytes\n", res->written);
        intv_bytes  += res->written;
        batch_bytes += res->written;
        intv_samples++;

        if(res->status == CAMIO_EBUFFRELEASED){
            DBG("Buffer at with id %lli (index =%lli) cannot be resused has been auto-released.\n", data_msgs[i].id, i);
            buff_states[buff_id].type = CAMIO_MSG_TYPE_NONE;
            continue;
        }

        //Convert this write data response into a write data request!
        buff_states[buff_id].type = CAMIO_MSG_TYPE_WRITE_DATA_REQ;
        reusable_buffs++;        
        ch_perf_event_stop(51,1,0);
    }
    ch_perf_event_stop(51,2,0);

    DBG("Sent %lli bytes, in %lli messages with %lli reusable bufferers\n", batch_bytes, data_msgs_len, reusable_buffs);
    if(reusable_buffs){
        DBG("Trying to reuse %lli buffers\n", reusable_buffs);
        send_perf_messages(muxable->parent.channel, id);
    }

    ch_perf_event_stop(5,3,0);

    //Try to get some new buffers now just in case
    get_new_buffers(muxable->parent.channel, id);

    ch_perf_event_stop(5,4,0);
    return CAMIO_ENOERROR;
}


static camio_error_t send_perf_messages(camio_channel_t* channel, ch_word chan_id)
{
    DBG("#### Trying to send message on chan_id=%lli!\n", chan_id);
    ch_perf_event_start(4,0,chan_id);
    ch_word to_send = 0;
    ch_word inflight_bytes = 0;
    camio_msg_t* buff_states = chan_states[chan_id].buff_states;
    for(ch_word i = 0; i < MIN(BUFFS_MAX,options.batching) && to_send < MSGS_MAX; i++){
        switch(buff_states[i].type){
            case CAMIO_MSG_TYPE_WRITE_DATA_REQ:{

                camio_wr_data_req_t* req = &buff_states[i].wr_data_req;
                camio_perf_packet_head_t* head = req->buffer->data_start;
                head->size = req->buffer->data_len;
                head->seq_number = seq;

                //struct timeval now = {0};
                //gettimeofday(&now, NULL);
                //time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
                ch_perf_cycles_now(time_now_ns);
                head->time_stamp = time_now_ns;

                seq++;
                inflight_bytes += req->buffer->data_len;

                data_msgs[to_send] = buff_states[i];
                to_send++;


                //hexdump(head,sizeof(*head) * 2);
                continue;
            }
            default:{}
        }
    }
    ch_perf_event_start(4,1,chan_id);
    data_msgs_len = to_send;
    DBG("Trying to send %lli bytes in %lli messages\n", inflight_bytes, data_msgs_len);
    camio_error_t err = camio_chan_wr_data_req(channel,data_msgs,&data_msgs_len);
    if(unlikely(err)){
        DBG("Could not request data writes with error %lli\n", err);
        ch_perf_event_stop(4,0,chan_id);
        return err;
    }
    ch_perf_event_start(4,2,chan_id);
    DBG("Successfully issued %lli/%lli data write requests\n", data_msgs_len, to_send);

    //Deal with the immidiate result of sending the messages, will still need to wait for the responses
    for(ch_word i = 0 ; i < data_msgs_len; i++ ){
        const ch_word buff_id = data_msgs[i].id;
        buff_states[buff_id] = data_msgs[i];
    }

    ch_perf_event_stop(4,1,data_msgs_len);
    return CAMIO_ENOERROR;
}


static inline void prepare_data_msg(camio_msg_t* msg)
{
    ch_perf_event_start(3,0,0);
    camio_wr_buff_res_t* wr_buff_res = &msg->wr_buff_res;

    //Figure out how much we should send with some basic sanity checking
    const ch_word req_bytes     = MAX((size_t)options.len, sizeof(camio_perf_packet_head_t));
    const ch_word bytes_to_send = MIN(req_bytes,wr_buff_res->buffer->data_len);

    msg->type = CAMIO_MSG_TYPE_WRITE_DATA_REQ;
    camio_wr_data_req_t* wr_data_req = &msg->wr_data_req;
    //Put data in the packet from the packet template
    //memcpy(wr_data_req->buffer->data_start, packet_data, bytes_to_send);
    wr_data_req->buffer->data_len = bytes_to_send;
    wr_data_req->dst_offset_hint = CAMIO_WRITE_REQ_DST_OFFSET_NONE;
    wr_data_req->src_offset_hint = CAMIO_WRITE_REQ_SRC_OFFSET_NONE;
    ch_perf_event_stop(3,0,bytes_to_send);
}


static camio_error_t on_new_wr_buffs(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Handling new buffs on chan_id=%lli\n", id);
    ch_perf_event_start(2,0,id);

    //Currently ignoring this values
    (void)usr_state;

    if(unlikely(err)){
        DBG("Unexpected error %lli\n", err);
        ch_perf_event_stop(2,0,id);
        return err;
    }

    data_msgs_len = MIN(MSGS_MAX,options.batching);
    err = camio_chan_wr_buff_res(muxable->parent.channel, data_msgs, &data_msgs_len);
    if(unlikely(err)){
        ERR("Could not get a writing buffers\n");
        ch_perf_event_stop(2,1,id);
        return CAMIO_EINVALID;
    }
    ch_perf_event_stop(2,2,id);
    DBG("Got %lli new writing buffers\n", data_msgs_len);
    //Yay! We got the responses, update the buffer states.
    camio_msg_t* buff_states = chan_states[id].buff_states;
    for(ch_word i = 0 ; i < data_msgs_len; i++ ){
        const ch_word buff_id = data_msgs[i].id;
        buff_states[buff_id] = data_msgs[i];

        if(unlikely(data_msgs[i].type != CAMIO_MSG_TYPE_WRITE_BUFF_RES)){
            WARN("Expected to get buffer  (id=%lli) response message (%lli), but got %lli instead.\n",
                    data_msgs[i].id, CAMIO_MSG_TYPE_WRITE_BUFF_RES, data_msgs[i].type);
            continue;
        }

        camio_wr_buff_res_t* res = &data_msgs[i].wr_buff_res;
        if(unlikely(res->status)){
            ERR("Error %lli getting buffer with id=%lli\n", res->status, data_msgs[i].id);
            data_msgs[i].type = CAMIO_MSG_TYPE_ERROR;
            data_msgs[i].err_res.status = res->status;
            continue;
        }

        //Woot! This looks like a valid buffer!
        DBG("Preparing message at buff_state idx=%lli\n", buff_id);
        prepare_data_msg(&buff_states[buff_id]);
    }
    ch_perf_event_stop(2,3,id);

    err = send_perf_messages(muxable->parent.channel, id);
    ch_perf_event_stop(2,4,id);
    return err;
}


static camio_error_t get_new_buffers(camio_channel_t* channel, ch_word chan_id)
{
    ch_perf_event_start(1,0,0);
    DBG("Getting buffers on chan_id=%lli!\n", chan_id);
    //Initialize a batch of messages based on the current buffer state
    ch_word to_send = 0;
    camio_msg_t* buff_states = chan_states[chan_id].buff_states;
    for(ch_word i = 0; i < MIN(BUFFS_MAX,options.batching) && to_send < MSGS_MAX; i++){
        switch(buff_states[i].type){
            case CAMIO_MSG_TYPE_NONE:{}
            //no break
            case CAMIO_MSG_TYPE_IGNORE:{}
            //no break
            case CAMIO_MSG_TYPE_WRITE_BUFF_REQ:{}
            //no break
            case CAMIO_MSG_TYPE_ERROR:{
                buff_states[i].type = CAMIO_MSG_TYPE_WRITE_BUFF_REQ;
                buff_states[i].id = i;
                data_msgs[to_send] = buff_states[i];
                to_send++;
                break;
            }
            default:{}
        }
    }

    if(eqlikely(to_send == 0)){
        //There's no work for us to do, exit now
        DBG("No work to do, exiting now\n");
        ch_perf_event_stop(1,0,0);
        return CAMIO_ENOERROR;
    }

    data_msgs_len = to_send;
    DBG("Requesting %lli wirte_buffs\n", data_msgs_len);
    camio_error_t err = camio_chan_wr_buff_req( channel, data_msgs, &data_msgs_len);
    if(unlikely(err)){
        DBG("Could not request buffers with error %lli\n", err);
        ch_perf_event_stop(1,1,0);
        return err;
    }
    DBG("Successfully issued %lli/%lli buffer requests\n", data_msgs_len, to_send);

    //The result of sending the messages, will still need to wait for the responses
    for(ch_word i = 0 ; i < data_msgs_len; i++ ){
        const ch_word buff_id = data_msgs[i].id;
        buff_states[buff_id] = data_msgs[i];
    }

    ch_perf_event_stop(1,2,data_msgs_len);
    return CAMIO_ENOERROR;
}

static camio_error_t on_new_rd_buffs(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Got new read buffers?? I didn't expect to get these?\n");
    (void)muxable;
    (void)err;
    (void)usr_state;
    (void)id;
    return CAMIO_EINVALID;
}

static camio_error_t on_new_rd_datas(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Got new read data?? I didn't expect to get these?\n");
    (void)muxable;
    (void)err;
    (void)usr_state;
    (void)id;
    return CAMIO_EINVALID;
}


static camio_error_t on_new_channels(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    //DBG("Handling new channels\n");

    //Currently ignoring these values
    (void)usr_state;
    (void)id;

    if(unlikely(err)){
        ERR("Unexpected error %lli\n", err);
        camio_mux_remove(mux,muxable);
        return err;
    }

    chan_msgs_len = MSGS_MAX;
    err = camio_ctrl_chan_res(muxable->parent.dev, chan_msgs, &chan_msgs_len );
    if(unlikely(err)){
        return err;
    }

    if(unlikely(chan_msgs_len == 0)){
        ERR("Got no new connections and no error?\n");
        return CAMIO_EINVALID;
    }

    ch_bool ask_for_more_chans = true; //Should we ask for some more channels?

    for(ch_word i = 0; i < chan_msgs_len; i++){
        const ch_word chan_id      = chan_msgs[i].id; //Get the channel id
        DBG("Chan id=$lli\n", chan_id);
        chan_states[chan_id].state = chan_msgs[i];    //Update the channel state to the latest state info

        switch(chan_msgs[i].type){
            case CAMIO_MSG_TYPE_IGNORE:{
                WARN("Ignoring channel response message at index %lli\n", i);
                continue;
            }
            case CAMIO_MSG_TYPE_ERROR:{
                ERR("Error %i in channel response message at index %lli\n", i, chan_msgs[i].err_res.status);
                continue;
            }
            case CAMIO_MSG_TYPE_CHAN_RES:{
                camio_chan_res_t* res = &chan_msgs[i].ch_res;
                if(unlikely(res->status == CAMIO_EALLREADYCONNECTED)){
                    //DBG("No more connections on this device\n", err);
                    camio_mux_remove(mux,muxable);
                    ask_for_more_chans = false;
                    continue;
                }

                if(unlikely(res->status)){
                    ERR("Could not get channel. Removing broken device with error=%li\n", res->status);
                    camio_mux_remove(mux,muxable);
                    ask_for_more_chans = false;
                    continue;
                }

                camio_mux_insert(mux,&res->channel->rd_data_muxable,on_new_rd_datas, NULL, chan_id );
                camio_mux_insert(mux,&res->channel->rd_buff_muxable,on_new_rd_buffs, NULL, chan_id );
                camio_mux_insert(mux,&res->channel->wr_data_muxable,on_new_wr_datas, NULL, chan_id );
                camio_mux_insert(mux,&res->channel->wr_buff_muxable,on_new_wr_buffs, NULL, chan_id );

                //Ask for some new write buffers on the channel
                get_new_buffers(res->channel, chan_id);
                break;
            }
            default:{
                ERR("Expected to get connect response message (%i), but got %i instead.\n",
                        CAMIO_MSG_TYPE_CHAN_RES, chan_msgs[i].type);
                chan_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
                DBG("WTF??? !!!");
                exit(256);
            }
        }
    }

    started = true; //Tell the stats to get going!

    if(ask_for_more_chans){
        return get_new_channels();
    }

    return CAMIO_ENOERROR;

}


static camio_error_t get_new_channels()
{
    //Initialize a batch of messages based on the current channel state to ask for more channels
    ch_word to_send = 0;
    for(ch_word i = 0; i < MIN(CHANS_MAX,options.batching) && to_send < MSGS_MAX; i++){
        switch(chan_states[i].state.type){
            case CAMIO_MSG_TYPE_NONE:{}
            //no break
            case CAMIO_MSG_TYPE_IGNORE:{}
            //no break
            case CAMIO_MSG_TYPE_CHAN_REQ:{}
            //no break
            case CAMIO_MSG_TYPE_ERROR:{
                chan_states[i].state.type = CAMIO_MSG_TYPE_CHAN_REQ;
                chan_states[i].state.id = i;
                chan_msgs[to_send] = chan_states[i].state;
                to_send++;
                break;
            }
            default:{}
        }
    }

    //Send those messages
    chan_msgs_len = to_send;
    DBG("Requesting %lli channels\n", chan_msgs_len);
    camio_error_t err = camio_ctrl_chan_req(delim, chan_msgs, &chan_msgs_len);
    if(unlikely(err)){
        ERR("Could not request channels with error %lli\n", err);
        return err;
    }
    DBG("Got %lli/%lli channel queue messages\n", chan_msgs_len, to_send);

    //Handle the result of sending the messages (note, will still have to wait for response as above)
    for(ch_word i = 0 ; i < chan_msgs_len; i++ ){
        const ch_word chan_id = chan_msgs[i].id;
        DBG("Chan id=%lli\n", chan_id);
        chan_states[chan_id].state = chan_msgs[i];
    }
    return CAMIO_ENOERROR;
}

camio_error_t init_packet_data()
{
    //Initialize a packet with some non zero junk
    packet_data = (char*)calloc(1,options.len);
    if(!packet_data){
        DBG("Could not allocate temporary packet data memory\n");
        return CAMIO_ENOMEM;
    }
    for(int i = 0; i < options.len; i++){
        packet_data [i] = i % 26 + 'A';
    }

    return CAMIO_ENOERROR;
}


camio_error_t get_new_delim(char* client_channel_uri)
{
    //Construct a delimiter
    camio_error_t err = connect_delim(client_channel_uri, &delim);
    if(err){
        DBG("Could not create delimiter! Err=%i\n", err);
        return CAMIO_EINVALID;
    }

    //Insert the device into the mux
    err = camio_mux_insert(mux,&delim->muxable, on_new_channels, NULL, 0);
    if(err){
        DBG("Could not insert device into multiplexer\n");
        return CAMIO_EINVALID;
    }

    return CAMIO_ENOERROR;
}


int camio_perf_clinet(ch_cstr client_channel_uri, ch_word* stop)
{
    DBG("Staring CamIO-perf client\n");

    //Create a new multiplexer for channels to go into
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);
    if(err){
        DBG("Could not create multiplexer. Err=%i\n", err);
        return err;
    }
    //Get the delimiter device that we will use
    err = get_new_delim(client_channel_uri);
    if(err){
        DBG("Could not get CamIO device with uri=%s. err=%i\n", client_channel_uri, err );
        return err;
    }
    //Set up the a template for packet data. This is so we don't do a modulus on the critical path
    err = init_packet_data();
    if(err){
        DBG("Error initializing the template packet data. err=%i\n", err);
        return err;
    }
    //Now ask for some channels, we'll get them back later in an event on the multiplexer
    err = get_new_channels();
    if(err){
        DBG("Could not request new channels to write to. Err=%i\n", err);
        return err;
    }


    //Set up some stats
    struct timeval now = {0};
    ch_word time_interval_ns    = 1000 * 1000 * 1000;
    ch_word total_bytes         = 0;
    ch_word time_start_ns       = -1;
    ch_word time_int_start_ns   = -1;

    //Ready to go!
    DBG("Staring main loop\n");
    camio_muxable_t* muxable     = NULL;
    while(!*stop && camio_mux_count(mux)){

        //Set up the start of sampling once we have a connection to a channel
        if(started && time_start_ns == -1){
            gettimeofday(&now, NULL);
            time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
            time_start_ns       = time_now_ns;
            time_int_start_ns   = time_now_ns;
        }

        gettimeofday(&now, NULL);
        time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
        if(started && time_now_ns >= time_int_start_ns + time_interval_ns){
            time_int_start_ns       = time_now_ns;
            total_bytes             += intv_bytes;
            ch_word total_time_ns   = time_now_ns - time_start_ns;
            double inst_rate_mbs    = ((double)intv_bytes / (double)time_interval_ns) * 1000;
            double ave_rate_mbs     = ((double)total_bytes / (double)total_time_ns) * 1000;
            double sample_rate_mps  = ((double)intv_samples / (double)time_interval_ns) * 1000;

            printf("#### inst rate=%3.3fMbs, samples=%lli, sample_rate=%3.3fM/s average rate=%3.3fMBs\n",
                inst_rate_mbs * 8,
                intv_samples,
                sample_rate_mps,
                ave_rate_mbs
            );

            intv_bytes  = 0;
            intv_samples = 0;

        }

        //Block waiting for a channel to be ready to read or connect this is a compromise between timing accuracy and speed
        camio_mux_select(mux,NULL,&muxable);

    }

    return 0;

}
