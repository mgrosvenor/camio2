/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   May 2, 2015
 *  File name: camio_udp_test.c
 *  Description:
 *  Test the features of the CamIO API regardless of the stream used
 */

#include <stdio.h>
#include <src/api/api_easy.h>
#include <src/camio_debug.h>
#include <src/drivers/delimiter/delim_transport.h>

#include <deps/chaste/data_structs/vector/vector_std.h>
#include <deps/chaste/options/options.h>
#include <deps/chaste/log/log.h>

USE_CAMIO;
USE_CH_LOGGER_DEFAULT;
USE_CH_OPTIONS;

struct {
    CH_VECTOR(cstr)*  opt6;
    CH_VECTOR(cstr)*  inout;
} options;


camio_error_t populate_mux( camio_mux_t* mux, CH_VECTOR(cstr)* uris, ch_word* mux_id_o) {
    DBG("populate mux count=%lli\n", uris->count);
    for (int i = 0; i < uris->count; i++) {
        ch_cstr* uri = uris->off(uris, i);
        camio_controller_t* connector = NULL;
        DBG("Adding connector with uri=%s\n", *uri);
        camio_error_t err = camio_connector_new(*uri, &connector);
        if (err) {
            ERR("Could not created connector\n");
            return CAMIO_EINVALID; //TODO should have a better undwind process here...
        }

        camio_mux_insert(mux, &connector->muxable, *mux_id_o);
        mux_id_o++;
    }

    return CAMIO_ENOERROR;
}


int main(int argc, char** argv)
{
    ch_opt_addSI(CH_OPTION_UNLIMTED,'i',"io","list of input/output streams", &options.inout,"stdio");

    ch_opt_parse(argc,argv);

    //Create a new multiplexer for streams to go into
    camio_mux_t* rc_mux = NULL;
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &rc_mux);
    if(err){
        ERR("Could not make new multiplexer\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }

    camio_mux_t* wr_mux = NULL;
    err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &wr_mux);
    if(err){
        ERR("Could not make new multiplexer\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }


    ch_word mux_id = 0;
    DBG("populating mux\n");
    err = populate_mux(rc_mux, options.inout, &mux_id);
    if(err){
        ERR("Error populating inout URIs \n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }


   //Read and write bytes to and from the stream
    camio_rd_buffer_t* rd_buffer = NULL;
    camio_rd_buffer_t* wr_buffer = NULL;
    camio_muxable_t* muxable     = NULL;
    ch_word which                = -1;
    camio_read_req_t  rd_req = {
        .src_offset_hint = CAMIO_READ_REQ_SRC_OFFSET_NONE,
        .dst_offset_hint = CAMIO_READ_REQ_DST_OFFSET_NONE,
        .read_size_hint  = CAMIO_READ_REQ_SIZE_ANY
    };
    camio_write_req_t  wr_req = {
        .src_offset_hint = CAMIO_WRITE_REQ_SRC_OFFSET_NONE,
        .dst_offset_hint = CAMIO_WRITE_REQ_DST_OFFSET_NONE,
    };


    CH_VECTOR(voidp)* streams = CH_VECTOR_NEW(voidp,128,CH_VECTOR_CMP(voidp));
    ch_word outstanding_writes = 0;
    while(camio_mux_count(rc_mux)){
        //Block waiting for a stream to be ready
        err = camio_mux_select(rc_mux,&muxable,&which);
        if(err != CAMIO_ENOERROR && err != CAMIO_ENOMORE){
            ERR("Unexpected error %lli on stream with id =%lli\n", err, which);
            continue;
        }

        //Handle connectors that are ready to connect
        switch(muxable->mode){
            case CAMIO_MUX_MODE_CONNECT:{
                DBG("Connect event\n");
                camio_stream_t* stream;
                if(err == CAMIO_ENOMORE){ //This connector is done, remove it
                    DBG("The connection is closed, removing it\n");
                    camio_mux_remove(rc_mux,muxable);
                    continue; //The connection is dead!
                }

                err = camio_connect(muxable->parent.connector,&stream);
                if(err){
                    ERR("Cannot connect to stream with id=%lli\n", which );
                    continue;
                }

                camio_mux_insert(rc_mux, &stream->rd_muxable, mux_id);
                mux_id++;
                camio_mux_insert(wr_mux, &stream->wr_muxable, mux_id);
                mux_id++;
                streams->push_back(streams,(void*)stream);

                DBG("req.off=%lli req.dst=%lli req.size=%lli\n",rd_req.src_offset_hint, rd_req.dst_offset_hint, rd_req.read_size_hint);
                err = camio_read_request(stream,&rd_req,1); //Tell the read stream that we would like some more data..
                if(err){ DBG("Got a read request error %i\n", err); return -1; }

                break;
            }

            case CAMIO_MUX_MODE_READ:{
                DBG("Read event outstanding=%lli\n", outstanding_writes);
                if(outstanding_writes > 0){
                    DBG("New read data, but waiting to finish writing\n");
                    continue;
                }

                camio_stream_t* stream = muxable->parent.stream;

                //Acquire a pointer to the new data now that it's ready
                DBG("Handling read event\n");
                err = camio_read_acquire(stream, &rd_buffer);
                if(err){ DBG("Got a read error %i\n", err); return -1; }
                DBG("Got %lli bytes of new data at %p\n", rd_buffer->data_len, rd_buffer->data_start);

                if(rd_buffer->data_len == 0){
                    DBG("The connection is closed, removing it\n");
                    camio_mux_remove(rc_mux,muxable);
                    continue; //The connection is dead!
                }

                for(int i = 0; i < streams->count; i++){
                    camio_stream_t** out = (camio_stream_t**)streams->off(streams,i);
                    camio_stream_t* out_stream = *out;

                    if(!out_stream->wr_muxable.vtable.ready){
                        //This stream does not have a writing end
                        continue;
                    }

                    //Get a buffer for writing stuff to
                    err = camio_write_acquire(out_stream, &wr_buffer);
                    if(err){ DBG("Could acquire new write buffer %lli\n", err); continue; }

                    //Copy data over from the read buffer to the write buffer
                    err = camio_copy_rw(&wr_buffer,&rd_buffer,0,0,rd_buffer->data_len);
                    if(err){ DBG("Got a copy error %i\n", err); continue; }

                    //Data copied, let's commit it and send it off
                    wr_req.buffer = wr_buffer;
                    err = camio_write_request(out_stream, &wr_req,1);
                    if(err){ DBG("Got a write request error %i\n", err); continue; }

                    //Write request has been queued up
                    outstanding_writes++;
                }

                //And we're done with the read buffer now, release it
                err = camio_read_release(stream, &rd_buffer);
                if(err){ DBG("Got a read release error %i\n", err); return -1; }
                DBG("Queued %lli write events\n", outstanding_writes);


                //Now wait for all of the writes to complete
                while(outstanding_writes){
                    err = camio_mux_select(wr_mux,&muxable, &which);
                    if(err){
                        if(err == CAMIO_ETRYAGAIN){
                            continue;
                        }

                        if(err != CAMIO_ECLOSED){
                            ERR("Unexpected error %lli\n", err);
                        }

                        camio_mux_remove(wr_mux,muxable);
                        outstanding_writes--;
                    }

                    DBG("Write event outstanding=%lli\n", outstanding_writes);
                    camio_stream_t* stream = muxable->parent.stream;
                    DBG("Handling write event\n");
                    //Done with the write buffer, release it
                    err = camio_write_release(stream,&wr_buffer);
                    if(err){ DBG("Got a write release error %i\n", err);  }
                    outstanding_writes--;
                }

                DBG("req.off=%lli req.dst=%lli req.size=%lli\n",rd_req.src_offset_hint, rd_req.dst_offset_hint, rd_req.read_size_hint);
                err = camio_read_request(stream,&rd_req,1); //Tell the read stream that we would like some more data..
                if(err){ DBG("Got a read request error %i\n", err); return -1; }
                break;

            }
            default:
                DBG("EeeK! Unknown mux type=%lli\n", muxable->mode);
        }
    }

    //TODO should really do clean up here...

    DBG("Terminating normally\n");
    return 0;
}
