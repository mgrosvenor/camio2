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
#include <src/api/api_easy.h>
#include <src/camio_debug.h>
#include <deps/chaste/chaste.h>

#include "options.h"
extern struct options_t options;

#define CONNECTOR_ID 0


int camio_perf_clinet(ch_cstr client_stream_uri, ch_word* stop)
{
    DBG("Staring CamIO-perf client\n");
    //Create a new multiplexer for streams to go into
    camio_mux_t* mux = NULL;
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);

    //Parse up parameters
    void* params;
    ch_word params_size;
    ch_word id;
    err = camio_transport_params_new(client_stream_uri,&params, &params_size, &id);
    if(err){
        DBG("Could not convert parameters into structure\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    //Use the parameters structure to construct a new connector object
    camio_connector_t* connector = NULL;
    err = camio_transport_constr(id,&params,params_size,&connector);
    if(err){
        DBG("Could not construct connector\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    DBG("Got connector\n");

    //Insert the connector into the mux
    err = camio_mux_insert(mux,&connector->muxable, CONNECTOR_ID);
    if(err){
        DBG("Could not insert connector into multiplexer\n");
        return CAMIO_EINVALID;
    }

    camio_rd_buffer_t* wr_buffer = NULL;
    camio_muxable_t* which       = NULL;

    //Figure out how much data to send
    ch_word bytes_to_send = options.len;

    camio_stream_t* tmp_stream = NULL;
    CH_VECTOR(voidp)* streams = CH_VECTOR_NEW(voidp,64,NULL);
    if(!streams){
        DBG("Could not create streams vector\n");
        return CAMIO_EINVALID;
    }

    struct timeval now = {0};
    gettimeofday(&now, NULL);

    ch_word time_start_ns       = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
    ch_word time_int_start_ns   = time_start_ns;
    ch_word time_now_ns         = 0;
    ch_word time_interval_ns    = 1000 * 1000 * 1000;
    ch_word total_bytes         = 0;
    ch_word intv_bytes          = 0;

    DBG("Staring main loop\n");
    while(!*stop){

        gettimeofday(&now, NULL);
        time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
        if(time_now_ns >= time_int_start_ns + time_interval_ns){
            time_int_start_ns       = time_now_ns;
            total_bytes             += intv_bytes;
            ch_word total_time_ns   = time_now_ns - time_start_ns;
            double inst_rate_mbs    = ((double)intv_bytes / (double)time_interval_ns) * 1000;
            double ave_rate_mbs     = ((double)total_bytes / (double)total_time_ns) * 1000;

            printf("#### [%lli - %lli] Bytes sent=%lli, inst rate=%3.3fMBs, total_bytes sent=%lli, average rate=%3.3fMBs\n",
                time_start_ns,
                time_now_ns,
                intv_bytes,
                inst_rate_mbs,
                total_bytes,
                ave_rate_mbs
            );

            intv_bytes = 0;
        }

        //Block waiting for a stream to be ready
        camio_mux_select(mux,&which);

        //Do a read request on all streams -- We don't care about reading (yet)
//        for(void** it = streams->first; it != streams->end; it = streams->next(streams,it)){
//            camio_stream_t* tmp_stream = (camio_stream_t*)*it;
//            camio_read_request(tmp_stream,0,0);
//        }

        //Have a look at what we have
        switch(which->id){
            //This is a connector that's just fired
            case CONNECTOR_ID:{
                err = camio_connect(connector,&tmp_stream);
                if(err != CAMIO_ENOERROR){
                    DBG("Could not get stream. Got errr %i\n", err);
                    return CAMIO_EINVALID;
                }
                streams->push_back(streams,tmp_stream);
                //camio_mux_insert(mux,&tmp_stream->rd_muxable,CONNECTOR_ID + 1); -- We don't care about reading! (yet)
                camio_mux_insert(mux,&tmp_stream->wr_muxable,CONNECTOR_ID + 3);

                break;
            }
            default:{
                tmp_stream = which->parent.stream;
                if(!wr_buffer){
                    //Get and init a buffer for writing stuff to. Hang on to it if possible
                    DBG("Stream is ready for writing, getting buffer\n");
                    err = camio_write_acquire(tmp_stream, &wr_buffer);
                    if(err){
                        DBG("Could not get a writing buffer to stream\n");
                        return CAMIO_EINVALID; /*TODO XXX put a better error here*/
                    }
                    DBG("Got buffer of size %i\n", wr_buffer->buffer_len);

                    bytes_to_send = MIN(bytes_to_send,wr_buffer->buffer_len);

                    for(int i = 0; i < bytes_to_send; i++){
                        *((char*)wr_buffer->buffer_start + i) = i % 27 + 'A';
                    }
                    wr_buffer->data_start = wr_buffer->buffer_start;
                    wr_buffer->data_len   = bytes_to_send;
                }

                err = camio_write_commit(tmp_stream, &wr_buffer );
                if(err == CAMIO_ETRYAGAIN){
                    //DBG("Only wrote %i bytes from %i\n", options.len - wr_buffer->)
                    continue;
                }
                if(err){
                    DBG("Got a write commit error %i\n", err);
                    return -1;
                }

                intv_bytes += bytes_to_send;

            }

        }

        //Done with the write buffer, release it
        if(wr_buffer){
            camio_write_release(tmp_stream,&wr_buffer);
        }


    }

    return 0;

}
