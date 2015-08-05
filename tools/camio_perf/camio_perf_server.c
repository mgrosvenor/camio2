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
#include "camio_perf_server.h"
#include "options.h"
#include <deps/chaste/utils/debug.h>
#include <deps/chaste/perf/perf.h>


extern struct options_t options;
static camio_mux_t* mux                 = NULL;
static camio_dev_t* device   = NULL;
#define MSGS_MAX 1024
static camio_msg_t ctrl_msgs[MSGS_MAX]  = {{0}};
static ch_word ctrl_msgs_len            = MSGS_MAX;
static camio_msg_t data_msgs[MSGS_MAX]  = {{0}};
static ch_word data_msgs_len            = MSGS_MAX;
static ch_bool started                  = false;
ch_word seq_exp                         = 0;

//Statistics keeping
struct timeval now                      = {0};
static ch_word time_now_ns              = 0;
static ch_word intv_bytes               = 0;
//static ch_word intv_samples             = 0;
static ch_word lost_count               = 0;
static ch_word found_count              = 0;
static ch_word min_latency              = INT64_MAX;
static ch_word max_latency              = INT64_MIN;
static ch_word total_latency            = 0;

static camio_error_t get_new_buffers(camio_channel_t* channel);
static camio_error_t on_new_rd_buffs(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id);
static camio_error_t get_new_channels();

#define DEVICE_ID 0
//static ch_word delimit(char* buffer, ch_word len)
//{
//    DBG("Deliminting %p len=%lli\n", buffer, len);
//    if((size_t)len < sizeof(camio_perf_packet_head_t)){
//        return -1;
//    }
//
//    camio_perf_packet_head_t* head = (camio_perf_packet_head_t*)buffer;
//    if(len < head->size){
//        return -1;
//    }
//
//    DBG("delimt packet size=%lli\n", head->size);
//    return head->size;
//}

//delim_params_t delim_params;
static camio_error_t connect_delim(ch_cstr server_channel_uri, camio_dev_t** device) {
//
//    //Find the ID of the delim channel
//    ch_word id;
//    camio_error_t err = camio_device_get_id("delim", &id);
//
//    //Fill in the delim channel paramters structure
//    delim_params.base_uri = client_channel_uri;
//    delim_params.delim_fn = delimit;
//
//    DBG("delimter=%p\n", delimit);
//    ch_word params_size = sizeof(delim_params_t);
//    void* params = &delim_params;
//
//    //Use the parameters structure to construct a new device object
//    err = camio_device_constr(id, &params, params_size, device);
//    if (err) {
//        DBG("Could not construct device\n");
//        return err; //TODO XXX put a better error here
//    }
//
//    //And we're done!
//    DBG("Got device\n");

    camio_easy_device_new(server_channel_uri,device);
    return CAMIO_ENOERROR;
}

static camio_error_t on_new_wr_buffs(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Got new write buffers?? I didn't expect to get these?\n");
    (void)muxable;
    (void)err;
    (void)usr_state;
    (void)id;
    return CAMIO_EINVALID;
}

static camio_error_t on_new_wr_datas(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Got new write data?? I didn't expect to get these?\n");
    (void)muxable;
    (void)err;
    (void)usr_state;
    (void)id;
    return CAMIO_EINVALID;
}

static camio_error_t on_new_rd_datas(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    ch_perf_event_start(3,0,0);
    DBG("Read data complete()\n");

    if(unlikely(err)){
        DBG("Unexpected error %lli\n", err);
        ch_perf_event_stop(3,1,0);
        return err;
    }

    data_msgs_len = MIN(MSGS_MAX,options.batching);
    err = camio_chan_rd_data_res(muxable->parent.channel, data_msgs, &data_msgs_len );
    if(unlikely(err)){
        ch_perf_event_stop(3,2,0);
        return err;
    }

    if(unlikely(data_msgs_len == 0)){
        DBG("Got no new read completions?\n");
        ch_perf_event_stop(3,3,0);
        return CAMIO_EINVALID;
    }

    ch_word reusable_buffs = 0; //See how many buffers we can potentially reuse

    ch_perf_event_stop(3,4,0);
    for(ch_word i = 0; i < data_msgs_len; i++){

        if(data_msgs[i].type == CAMIO_MSG_TYPE_IGNORE){
            DBG("Ignoring message at index %lli\n", i);
            continue;
        }

        if(data_msgs[i].type != CAMIO_MSG_TYPE_READ_DATA_RES){
            ERR("Expected to get read data response message (%lli), but got %i instead.\n",
                    CAMIO_MSG_TYPE_READ_DATA_RES, data_msgs[i].type);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        camio_rd_buff_res_t* res = &data_msgs[i].rd_buff_res;
        if(res->status && res->status != CAMIO_ECANNOTREUSE){
            ERR("Error %i getting buffer with id=%lli\n", res->status, data_msgs[i].id);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        camio_buffer_t* buffer = res->buffer;
        DBG("Received %i bytes\n", buffer->data_len);
        if(buffer->data_len <= 0){
            ERR("Stream has ended, removing\n");
            camio_mux_remove(mux, muxable);
            continue;
        }

        intv_bytes += buffer->data_len;
        camio_perf_packet_head_t* head = (camio_perf_packet_head_t*)buffer->data_start;
        //hexdump(head,sizeof(*head) * 2);

        if(head->seq_number != seq_exp){
            ERR("Sequence number error, expected to find %lli but found %lli\n", seq_exp, head->seq_number);
            lost_count++;
        }
        else{
            found_count++;
            //gettimeofday(&now, NULL);
            //time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
            ch_perf_cycles_now(time_now_ns);
            const ch_word latency = time_now_ns - head->time_stamp;
            min_latency = MIN(min_latency, latency);
            max_latency = MAX(max_latency, latency);
            total_latency += latency;
        }
        seq_exp = head->seq_number + 1;

        if(res->status == CAMIO_ECANNOTREUSE){
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            err = camio_chan_rd_buff_release(muxable->parent.channel, buffer);
            if(err){
                DBG("WTF? Error releasing buffer??\n");
                return err;//Don't know how to recover from this!
            }
            res->buffer = NULL;
            ch_perf_event_stop(3,5,0);
            continue;
        }

        //If we get here, the buffer can be used again.
        reusable_buffs++;
        ch_perf_event_stop(3,6,0);
    }

    if(reusable_buffs){
        ch_perf_event_start(3,7,0);
        on_new_rd_buffs(muxable, 0, usr_state, id);
        ch_perf_event_stop(3,8,0);
    }
    else{
        ch_perf_event_start(3,9,0);
        get_new_buffers(muxable->parent.channel);
        ch_perf_event_stop(3,10,0);
    }

    ch_perf_event_stop(3,11,0);

    return CAMIO_ENOERROR;
}


static camio_error_t on_new_rd_buffs(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Handling new buffs\n");
    ch_perf_event_start(2,0,0);
    //Currently ignoring these values
    (void)usr_state;
    (void)id;

    if(unlikely(err)){
        DBG("Unexpected error %lli\n", err);
        ch_perf_event_stop(2,1,0);
        return err;
    }

    data_msgs_len = MIN(MSGS_MAX,options.batching);
    err = camio_chan_rd_buff_res(muxable->parent.channel, data_msgs, &data_msgs_len);
    if(unlikely(err)){
        ERR("Could not get a writing buffers\n");
        ch_perf_event_stop(2,2,0);
        return err;
    }

    for(ch_word i = 0; i < data_msgs_len; i++){

        if(data_msgs[i].type == CAMIO_MSG_TYPE_IGNORE){
            DBG("Ignoring read buffer at index %lli\n", i);
            continue;
        }

        if(data_msgs[i].type != CAMIO_MSG_TYPE_READ_BUFF_RES){
            ERR("Expected to get read buffer response message (%lli), but got %i instead.\n",
                    CAMIO_MSG_TYPE_READ_BUFF_RES, data_msgs[i].type);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        camio_rd_buff_res_t* res = &data_msgs[i].rd_buff_res;
        if(res->status){
            ERR("Error %i getting buffer with id=%lli\n", res->status, data_msgs[i].id);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        data_msgs[i].type = CAMIO_MSG_TYPE_READ_DATA_REQ;
        camio_rd_data_req_t* req = &data_msgs[i].rd_data_req;

        req->dst_offset_hint = CAMIO_READ_REQ_DST_OFFSET_NONE;
        req->src_offset_hint = CAMIO_READ_REQ_SRC_OFFSET_NONE;
        req->read_size_hint  = CAMIO_READ_REQ_SIZE_ANY;
        ch_perf_event_start(2,3,0);
    }

    ch_perf_event_stop(2,4,0);
    err = camio_chan_rd_data_req(muxable->parent.channel, data_msgs, &data_msgs_len);
    if(unlikely(err)){
        ERR("Could not request read data\n");
        ch_perf_event_stop(2,5,0);
        return err;
    }

    return err;
}

static camio_error_t get_new_buffers(camio_channel_t* channel)
{
    ch_perf_event_start(1,0,0);
    //Initialize a batch of messages to request some write buffers
    for(int i = 0; i < MIN(MSGS_MAX,options.batching); i++){
        data_msgs[i].type = CAMIO_MSG_TYPE_READ_BUFF_REQ;
        data_msgs[i].id = i;
        camio_rd_buff_req_t* req = &data_msgs[i].rd_buff_req;
        req->dst_offset_hint = CAMIO_READ_REQ_DST_OFFSET_NONE;
        req->src_offset_hint = CAMIO_READ_REQ_SRC_OFFSET_NONE;
        req->read_size_hint  = CAMIO_READ_REQ_SIZE_ANY;
    }

    data_msgs_len = MIN(MSGS_MAX,options.batching);
    DBG("Requesting %lli read_buffs\n", data_msgs_len);
    camio_error_t err = camio_chan_rd_buff_req( channel, data_msgs, &data_msgs_len);
    if(unlikely(err)){
        ERR("Could not request buffers with error %lli\n", err);
        ch_perf_event_stop(1,1,0);
        return err;
    }
    DBG("Successfully issued %lli buffer requests\n", data_msgs_len);
    ch_perf_event_stop(1,2,0);
    return CAMIO_ENOERROR;
}


static camio_error_t on_new_channels(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    //DBG("Handling got new connect\n");

    //Currently ignoring these values
    (void)usr_state;
    (void)id;

    if(err){
        camio_mux_remove(mux,muxable);
        ERR("Unexpected error %lli\n", err);
        return err;
    }

    ctrl_msgs_len = MIN(MSGS_MAX,options.batching);
    err = camio_ctrl_chan_res(muxable->parent.dev, ctrl_msgs, &ctrl_msgs_len );
    if(err){
        return err;
    }

    //DBG("Got %lli new channels\n", ctrl_msgs_len);

    if(ctrl_msgs_len == 0){
        DBG("Got no new connections?\n");
        return CAMIO_EINVALID;
    }

    for(ch_word i = 0; i < ctrl_msgs_len; i++){
        if(ctrl_msgs[i].type == CAMIO_MSG_TYPE_IGNORE){
            ERR("Ignoring connect response message at index %lli\n", i);
            continue;
        }

        if(ctrl_msgs[i].type != CAMIO_MSG_TYPE_CHAN_RES){
            ERR("Expected to get connect response message (%lli), but got %lli instead.\n",
                    CAMIO_MSG_TYPE_CHAN_RES, ctrl_msgs[i].type);
            ctrl_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        camio_chan_res_t* res = &ctrl_msgs[i].ch_res;
        if(res->status == CAMIO_EALLREADYCONNECTED){
            //DBG("No more connections on this device\n", err);
            camio_mux_remove(mux,muxable);
            return CAMIO_ENOERROR; //Bail out here. The device is gone
        }

        if(res->status){
            ERR("Could not get channel. Removing broken device with error=%lli\n", err);
            camio_mux_remove(mux,muxable);
            return CAMIO_ENOERROR; //Bail out here. The device is gone
        }

        camio_mux_insert(mux,&res->channel->rd_data_muxable,on_new_rd_datas, NULL, DEVICE_ID + 1);
        camio_mux_insert(mux,&res->channel->rd_buff_muxable,on_new_rd_buffs, NULL, DEVICE_ID + 2);
        camio_mux_insert(mux,&res->channel->wr_data_muxable,on_new_wr_datas, NULL, DEVICE_ID + 3);
        camio_mux_insert(mux,&res->channel->wr_buff_muxable,on_new_wr_buffs, NULL, DEVICE_ID + 4);

        //Get the stream started by asking for some new buffers to read into
        get_new_buffers(res->channel);
    }

    started = true; //Tell the stats that we're going!
    //And see if we can grab some more
    return get_new_channels();

}


static camio_error_t get_new_channels()
{
    //DBG("Trying to get some new channels\n");
    //Initialize a batch of messages
    for(int i = 0; i < MIN(MSGS_MAX,options.batching); i++){
        ctrl_msgs[i].type = CAMIO_MSG_TYPE_CHAN_REQ;
        ctrl_msgs[i].id = i;
    }

    ctrl_msgs_len = MIN(MSGS_MAX,options.batching);
    //DBG("Requesting %lli channel\n", MSGS_MAX);
    camio_error_t err = camio_ctrl_chan_req(device, ctrl_msgs, &ctrl_msgs_len);

    if(err){
        ERR("Could not request channels with error %lli\n", err);
        return err;
    }
    //DBG("Successfully issued %lli channel requests\n", ctrl_msgs_len);

    return CAMIO_ENOERROR;
}



int camio_perf_server(ch_cstr client_channel_uri, ch_word* stop)
{
    DBG("Staring CamIO-perf Server\n");
    //Create a new multiplexer for channels to go into
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);

    //Construct a delimiter
    err = connect_delim(client_channel_uri, &device);
    if(err){
        DBG("Could not create delimiter!\n");
        return CAMIO_EINVALID;
    }

    //Insert the device into the mux
    err = camio_mux_insert(mux,&device->muxable, on_new_channels, NULL, DEVICE_ID);
    if(err){
        DBG("Could not insert device into multiplexer\n");
        return CAMIO_EINVALID;
    }


    struct timeval now = {0};
    ch_word time_interval_ns    = 1000 * 1000 * 1000;
    ch_word total_bytes         = 0;
    ch_word time_start_ns       = -1;
    ch_word time_int_start_ns   = -1;

    //Kick things off by requesting some connections
    get_new_channels();

    DBG("Staring main loop\n");
    camio_muxable_t* muxable     = NULL;


    while(!*stop && camio_mux_count(mux)){

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
            double loss_rate        = (double)lost_count / ((double)lost_count + (double)found_count) * 100;
            double ave_latency      = (double)total_latency / (double)found_count;

            printf("#### inst rate=%3.3fMbs, average rate=%3.3fMBs loss=%3.3f%% latency [%3.0f, %3.0f, %3.0f]ns\n",
                inst_rate_mbs * 8,
                ave_rate_mbs,
                loss_rate,
                (double)min_latency / 3.1,
                (double)ave_latency / 3.1,
                (double)max_latency / 3.1
            );

            lost_count          = 0;
            found_count         = 0;
            min_latency         = INT64_MAX;
            max_latency         = INT64_MIN;
            total_latency       = 0;
            intv_bytes = 0;
        }

        //Block waiting for something interesting to happen
        camio_mux_select(mux,NULL,&muxable);

    }


    return 0;

}
