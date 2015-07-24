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

#include "options.h"
extern struct options_t options;
static camio_mux_t* mux               = NULL;
static ch_word time_now_ns            = 0;
static camio_rd_req_t rreq            = {0};
static camio_controller_t* controller = NULL;
static camio_chan_req_t chan_req      = { 0 };

#include "camio_perf_server.h"

#define CONNECTOR_ID 0
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
static camio_error_t connect_delim(ch_cstr server_channel_uri, camio_controller_t** controller) {
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
//    //Use the parameters structure to construct a new controller object
//    err = camio_device_constr(id, &params, params_size, controller);
//    if (err) {
//        DBG("Could not construct controller\n");
//        return err; //TODO XXX put a better error here
//    }
//
//    //And we're done!
//    DBG("Got controller\n");

    camio_controller_new(server_channel_uri,controller);
    return CAMIO_ENOERROR;
}



static camio_error_t on_new_connect(camio_muxable_t* muxable)
{
    DBG("Handling got new connect\n");
    camio_chan_req_t* res;
    camio_error_t err = camio_ctrl_chan_res(muxable->parent.controller,&res);
    if(err){
        return err;
    }

    if(res->status == CAMIO_EALLREADYCONNECTED){
        DBG("No more connections on this controller\n", err);
        camio_mux_remove(mux,muxable);
        return CAMIO_ENOERROR;
    }

    if(res->status){
        DBG("Could not get channel. Removing broken controller with error=%lli\n", err);
        camio_mux_remove(mux,muxable);
        return res->status;
    }

    camio_mux_insert(mux,&res->channel->rd_muxable,CONNECTOR_ID + 1);
    camio_mux_insert(mux,&res->channel->wr_muxable,CONNECTOR_ID + 3);

    //Kick things off by asking for data
    rreq.dst_offset_hint = CAMIO_READ_REQ_DST_OFFSET_NONE;
    rreq.src_offset_hint = CAMIO_READ_REQ_SRC_OFFSET_NONE;
    rreq.read_size_hint  = CAMIO_READ_REQ_SIZE_ANY;
    err = camio_chan_rd_req(chan_req.channel,&rreq,1);
    if(err != CAMIO_ENOERROR){
        ERR("Could not issue read_request. Got err %lli\n", err);
        return CAMIO_EINVALID;
    }

    //We have a successful connection, what about another one?
    err = camio_ctrl_chan_req(controller, &chan_req, 1);
    if(err){
        ERR("Could not issue channel request. Got err %lli\n", err);
        return err;
    }

    return CAMIO_ENOERROR;

}



int camio_perf_server(ch_cstr client_channel_uri, ch_word* stop)
{
    DBG("Staring CamIO-perf Server\n");
    //Create a new multiplexer for channels to go into
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);

    //Construct a delimiter
    err = connect_delim(client_channel_uri, &controller);
    if(err){
        DBG("Could not create delimiter!\n");
        return CAMIO_EINVALID;
    }

    //Insert the controller into the mux
    err = camio_mux_insert(mux,&controller->muxable, CONNECTOR_ID);
    if(err){
        DBG("Could not insert controller into multiplexer\n");
        return CAMIO_EINVALID;
    }

    struct timeval now = {0};
    gettimeofday(&now, NULL);

    ch_word time_start_ns       = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
    ch_word time_int_start_ns   = time_start_ns;
    ch_word time_interval_ns    = 1000 * 1000 * 1000;
    ch_word total_bytes         = 0;
    ch_word intv_bytes          = 0;
    ch_word lost_count          = 0;
    ch_word found_count         = 0;
    ch_word min_latency         = INT64_MAX;
    ch_word max_latency         = INT64_MIN;
    ch_word total_latency       = 0;


    camio_ctrl_chan_req(controller, &chan_req, 1);

    DBG("Staring main loop\n");
    camio_muxable_t* muxable     = NULL;
    camio_rd_buffer_t* rd_buffer = NULL;
    ch_word which                = 0;
    ch_word seq_exp              = 0;

    while(!*stop){
        gettimeofday(&now, NULL);
        time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
        if(time_now_ns >= time_int_start_ns + time_interval_ns){
            time_int_start_ns       = time_now_ns;
            total_bytes             += intv_bytes;
            ch_word total_time_ns   = time_now_ns - time_start_ns;
            double inst_rate_mbs    = ((double)intv_bytes / (double)time_interval_ns) * 1000;
            double ave_rate_mbs     = ((double)total_bytes / (double)total_time_ns) * 1000;
            double loss_rate        = (double)lost_count / ((double)lost_count + (double)found_count) * 100;
            double ave_latency      = (double)total_latency / (double)found_count;

            printf("#### inst rate=%3.3fMbs, average rate=%3.3fMBs loss=%3.3f%% latency [%lli, %3.3f, %lli]us\n",
                inst_rate_mbs * 8,
                ave_rate_mbs,
                loss_rate,
                min_latency / 1000,
                ave_latency / 1000,
                max_latency / 1000
            );

            lost_count          = 0;
            found_count         = 0;
            min_latency         = INT64_MAX;
            max_latency         = INT64_MIN;
            total_latency       = 0;
            intv_bytes = 0;
        }

        //Block waiting for a channel to be ready to read or connect
        camio_mux_select(mux,&muxable,&which);

        //Have a look at what we have
        switch(muxable->mode){
            //This is a controller that's just fired
            case CAMIO_MUX_MODE_CONNECT:{
                err = on_new_connect(muxable);
                if(err){ return err; } //Cannot recover from connection error!
                break;
            }
            case CAMIO_MUX_MODE_READ:{
                DBG("Handling read ready()\n");
                camio_rd_req_t* res = NULL;
                DBG("res=%p\n");
                err = camio_chan_rd_res(muxable->parent.channel, &res );
                if(err != CAMIO_ENOERROR){
                    ERR("Could not acquire buffer. Got errr %i\n", err);
                    return CAMIO_EINVALID;
                }
                DBG("res=%p\n");
                if(res->status != CAMIO_ENOERROR){
                    DBG("Reading had an error %lli\n", res->status);
                    continue;
                }

                rd_buffer = res->buffer;

                DBG("Got %lli bytes\n", rd_buffer->data_len);
                if(rd_buffer->data_len <= 0){
                    DBG("Stream has ended, removing\n");
                    camio_mux_remove(mux, muxable);
                }

                intv_bytes += rd_buffer->data_len;
                camio_perf_packet_head_t* head = (camio_perf_packet_head_t*)rd_buffer->data_start;
                if(head->seq_number != seq_exp){
                    //ERR("Sequence number error, expected to find %lli but found %lli\n", seq_exp, head->seq_number);
                    lost_count++;
                }
                else{
                    found_count++;
                    const ch_word latency = time_now_ns - head->time_stamp;
                    min_latency = MIN(min_latency, latency);
                    max_latency = MAX(max_latency, latency);
                    total_latency += latency;
                }
                seq_exp = head->seq_number + 1;

                //printf("ts=%lli, len=%lli, seq=%lli, \n", head->time_stamp, head->size, head->seq_number );

                err = camio_chan_rd_release(muxable->parent.channel, rd_buffer);
                if(err){
                    DBG("WTF? Error releasing buffer??\n");
                    return err;//Don't know how to recover from this!
                }
                rd_buffer = NULL;

                rreq.dst_offset_hint = CAMIO_READ_REQ_DST_OFFSET_NONE;
                rreq.src_offset_hint = CAMIO_READ_REQ_SRC_OFFSET_NONE;
                rreq.read_size_hint  = CAMIO_READ_REQ_SIZE_ANY;
                err = camio_chan_rd_req(muxable->parent.channel,&rreq,1);
                if(err != CAMIO_ENOERROR){
                    ERR("Could not issue read_request. Got errr %i\n", err);
                    return CAMIO_EINVALID;
                }

                break;
            }
            case CAMIO_MUX_MODE_WRITE:{
                ERR("Handling write ready() -- I didn't expect this...\n");
                break;
            }
            default:{
                ERR("Um? What\n");
            }
        }

    }


    return 0;

}
