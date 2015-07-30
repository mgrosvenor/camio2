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
#include <src/camio_debug.h>
//#include <src/drivers/delimiter/delim_device.h>
#include "camio_perf_packet.h"
#include <src/devices/messages.h>

#include "options.h"
extern struct options_t options;

#define CONNECTOR_ID 0
static camio_mux_t* mux                 = NULL;
static camio_controller_t* controller   = NULL;

#define MSGS_MAX 1024
static camio_msg_t data_msgs[MSGS_MAX]  = {{0}};
static ch_word data_msgs_len            = MSGS_MAX;
static camio_msg_t ctrl_msgs[MSGS_MAX]  = {{0}};
static ch_word ctrl_msgs_len            = MSGS_MAX;
static char* packet_data                = NULL;
static ch_bool started                  = false;

//Statistics keeping
static ch_word time_now_ns              = 0;
static ch_word seq                      = 0;
static ch_word intv_bytes               = 0;
static ch_word intv_samples             = 0;

static camio_error_t get_new_buffers(camio_channel_t* channel);
static camio_error_t send_perf_messages(camio_channel_t* channel);
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

static camio_error_t connect_delim(ch_cstr client_channel_uri, camio_controller_t** controller) {

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
//    //Use the parameters structure to construct a new controller object
//    err = camio_device_constr(id, &params, params_size, controller);
//    if (err) {
//        DBG("Could not construct controller\n");
//        return CAMIO_EINVALID; //TODO XXX put a better error here
//    }
//
//    //And we're done!
//    DBG("Got controller\n");


    camio_controller_new(client_channel_uri,controller);

    return CAMIO_ENOERROR;
}


static camio_error_t on_new_wr_datas(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Handling write complete()\n");

    //Currently ignoring these values
    (void)usr_state;
    (void)id;

    if(err){
        DBG("Unexpected error %lli\n", err);
        return err;
    }

    data_msgs_len = MSGS_MAX;
    err = camio_chan_wr_data_res(muxable->parent.channel, data_msgs, &data_msgs_len );
    if(err){
        return err;
    }

    if(data_msgs_len == 0){
        DBG("Got no new write completions?\n");
        return CAMIO_EINVALID;
    }

    ch_word reusable_buffs = 0; //See how many buffers we can potentially reuse

    for(ch_word i = 0; i < data_msgs_len; i++){
        if(data_msgs[i].type == CAMIO_MSG_TYPE_IGNORE){
            DBG("Ignoring write response message at index %lli\n", i);
            continue;
        }

        if(data_msgs[i].type != CAMIO_MSG_TYPE_WRITE_DATA_RES){
            ERR("Expected to get write response message (%i) at [%lli], but got %i instead.\n",
                    CAMIO_MSG_TYPE_WRITE_DATA_RES, i, data_msgs[i].type);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        camio_wr_data_res_t* res = &data_msgs[i].wr_data_res;
        if(res->status != CAMIO_ENOERROR && res->status != CAMIO_EBUFFRELEASED){
            DBG("Failure to write data at [%lli] error=%i\n", i, err);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        DBG("Sent %lli bytes\n", res->written);
        intv_bytes +=  res->written;
        intv_samples++;

        if(res->status == CAMIO_EBUFFRELEASED){
            DBG("Buffer at index [%lli] cannot be resused has been auto-released.\n", i);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        reusable_buffs++;
    }


    if(reusable_buffs){
        send_perf_messages(muxable->parent.channel);
    }
    else{
        get_new_buffers(muxable->parent.channel);
    }

    return CAMIO_ENOERROR;
}


static camio_error_t send_perf_messages(camio_channel_t* channel)
{
    DBG("#### Trying to send message!\n");

    camio_error_t err = CAMIO_ENOERROR;

    ch_word inflight_bytes = 0;
    for(ch_word i = 0; i < data_msgs_len; i++){
        if(data_msgs[i].type == CAMIO_MSG_TYPE_IGNORE){
            DBG("Ignoring message at index %lli\n", i);
            continue;
        }

        camio_wr_data_req_t* req = &data_msgs[i].wr_data_req;
        camio_perf_packet_head_t* head = req->buffer->data_start;
        head->size = req->buffer->data_len;
        head->seq_number = seq;

        struct timeval now = {0};
        gettimeofday(&now, NULL);
        time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
        head->time_stamp = time_now_ns;

        DBG("Write request buffer %p of size %lli with data at %p\n",
                req->buffer,
                req->buffer->data_len,
                req->buffer->data_start
        );

        seq++;
        inflight_bytes = req->buffer->data_len;
    }

    DBG("Trying to send %lli bytes in %lli messages\n", inflight_bytes, data_msgs_len);
    err = camio_chan_wr_data_req(channel,data_msgs,&data_msgs_len);
    if(err != CAMIO_ENOERROR){
        ERR("Unexpected write error %lli\n", err);
        return err;
    }

    return CAMIO_ENOERROR;
}


static void prepare_data_msgs(void)
{
    for(ch_word i = 0; i < data_msgs_len; i++){

        if(data_msgs[i].type == CAMIO_MSG_TYPE_IGNORE){
            DBG("Ignoring message at index %lli\n", i);
            continue;
        }

        if(data_msgs[i].type != CAMIO_MSG_TYPE_WRITE_BUFF_RES){
            ERR("Expected to get buffer response message (%lli), but got %lli instead.\n",
                    CAMIO_MSG_TYPE_WRITE_BUFF_RES, data_msgs[i].type);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        camio_wr_buff_res_t* res = &data_msgs[i].wr_buff_res;
        if(res->status){
            ERR("Error %lli getting buffer with id=%lli\n", res->status, data_msgs[i].id);
            data_msgs[i].type = CAMIO_MSG_TYPE_IGNORE;
            continue;
        }

        data_msgs[i].type = CAMIO_MSG_TYPE_WRITE_DATA_REQ;
        camio_wr_data_req_t* req = &data_msgs[i].wr_data_req;

        //Figure out how much we should send. -- Just some basic sanity checking
        const ch_word req_bytes     = MAX((size_t)options.len, sizeof(camio_perf_packet_head_t));
        const ch_word bytes_to_send = MIN(req_bytes,req->buffer->data_len);

        //Put data in the packet from the temporary packetb
        memcpy(req->buffer->data_start, packet_data, bytes_to_send);
        req->buffer->data_len = bytes_to_send;
        req->dst_offset_hint = CAMIO_WRITE_REQ_DST_OFFSET_NONE;
        req->src_offset_hint = CAMIO_WRITE_REQ_SRC_OFFSET_NONE;
    }
}


static camio_error_t on_new_wr_buffs(camio_muxable_t* muxable, camio_error_t err, void* usr_state, ch_word id)
{
    DBG("Handling new buffs\n");

    //Currently ignoring these values
    (void)usr_state;
    (void)id;

    if(err){
        DBG("Unexpected error %lli\n", err);
        return err;
    }

    data_msgs_len = MSGS_MAX;
    err = camio_chan_wr_buff_res(muxable->parent.channel, data_msgs, &data_msgs_len);
    if(err){
        ERR("Could not get a writing buffers\n");
        return CAMIO_EINVALID;
    }

    DBG("Got %lli new writing buffers\n", data_msgs_len);
    prepare_data_msgs();

    DBG("Sending new buffers\n", data_msgs_len);
    return send_perf_messages(muxable->parent.channel);
}


static camio_error_t get_new_buffers(camio_channel_t* channel)
{

    //Initialize a batch of messages to request some write buffers
    for(int i = 0; i < MSGS_MAX; i++){
        data_msgs[i].type = CAMIO_MSG_TYPE_WRITE_BUFF_REQ;
        data_msgs[i].id = i;
    }

    data_msgs_len = MSGS_MAX;
    DBG("Requesting %lli wirte_buffs\n", MSGS_MAX);
    camio_error_t err = camio_chan_wr_buff_req( channel, data_msgs, &data_msgs_len);
    if(err){
        DBG("Could not request buffers with error %lli\n", err);
        return err;
    }
    DBG("Successfully issued %lli buffer requests\n", data_msgs_len);

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
    DBG("Handling got new connect\n");

    //Currently ignoring these values
    (void)usr_state;
    (void)id;

    if(err){
        DBG("Unexpected error %lli\n", err);
        return err;
    }

    ctrl_msgs_len = MSGS_MAX;
    err = camio_ctrl_chan_res(muxable->parent.controller, ctrl_msgs, &ctrl_msgs_len );
    if(err){
        return err;
    }

    if(ctrl_msgs_len == 0){
        DBG("Got no new connections?\n");
        return CAMIO_EINVALID;
    }

    for(ch_word i = 0; i < ctrl_msgs_len; i++){
        if(ctrl_msgs[i].type == CAMIO_MSG_TYPE_IGNORE){
            DBG("Ignoring connect response message at index %lli\n", i);
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
            DBG("No more connections on this controller\n", err);
            camio_mux_remove(mux,muxable);
            continue;
        }

        if(res->status){
            DBG("Could not get channel. Removing broken controller with error=%lli\n", err);
            camio_mux_remove(mux,muxable);
            continue;
        }

        camio_mux_insert(mux,&res->channel->rd_data_muxable,on_new_rd_datas, NULL, CONNECTOR_ID + 1);
        camio_mux_insert(mux,&res->channel->rd_buff_muxable,on_new_rd_buffs, NULL, CONNECTOR_ID + 2);
        camio_mux_insert(mux,&res->channel->wr_data_muxable,on_new_wr_datas, NULL, CONNECTOR_ID + 3);
        camio_mux_insert(mux,&res->channel->wr_buff_muxable,on_new_wr_buffs, NULL, CONNECTOR_ID + 4);

        //Kick things off by asking for some new write buffers on the channel
        get_new_buffers(res->channel);
    }

    started = true; //Tell the stats that we're going!

    return get_new_channels();

}


static camio_error_t get_new_channels()
{
    //Initialize a batch of messages
    for(int i = 0; i < MSGS_MAX; i++){
        ctrl_msgs[i].type = CAMIO_MSG_TYPE_CHAN_REQ;
        ctrl_msgs[i].id = i;
    }

    ctrl_msgs_len = MSGS_MAX;
    DBG("Requesting %lli wirte_buffs\n", MSGS_MAX);
    camio_error_t err = camio_ctrl_chan_req(controller, ctrl_msgs, &ctrl_msgs_len);

    if(err){
        DBG("Could not request channels with error %lli\n", err);
        return err;
    }
    DBG("Successfully issued %lli channel requests\n", ctrl_msgs_len);

    return CAMIO_ENOERROR;
}



int camio_perf_clinet(ch_cstr client_channel_uri, ch_word* stop)
{
    DBG("Staring CamIO-perf client\n");
    //Create a new multiplexer for channels to go into

    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);

    //Construct a delimiter
    err = connect_delim(client_channel_uri, &controller);
    if(err){
        DBG("Could not create delimiter!\n");
        return CAMIO_EINVALID;
    }

    //Insert the controller into the mux
    err = camio_mux_insert(mux,&controller->muxable, on_new_channels, NULL, CONNECTOR_ID);
    if(err){
        DBG("Could not insert controller into multiplexer\n");
        return CAMIO_EINVALID;
    }

    struct timeval now = {0};
    ch_word time_interval_ns    = 1000 * 1000 * 1000;
    ch_word total_bytes         = 0;
    ch_word time_start_ns       = -1;
    ch_word time_int_start_ns   = -1;


    //Initialize a packet with some non zero junk
    packet_data = (char*)calloc(1,options.len);
    if(!packet_data){
        DBG("Could not allocate temporary packet data memory\n");
        return CAMIO_ENOMEM;
    }
    for(int i = 0; i < options.len; i++){
        packet_data [i] = i % 26 + 'A';
    }

    get_new_channels();

    DBG("Staring main loop\n");
    camio_muxable_t* muxable     = NULL;

    while(!*stop){

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

        //Block waiting for a channel to be ready to read or connect
        camio_mux_select(mux,NULL,&muxable);

    }

//    if(wr_buffer){
//        camio_write_release(tmp_channel,&wr_buffer);
//    }


    return 0;

}
