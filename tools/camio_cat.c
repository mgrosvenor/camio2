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


USE_CAMIO;


#define DELIM_LEN 8
int delimit(char* buffer, int len)
{
    DBG("Running delimiter on buffer=%p with len=%i\n", buffer,len);
    (void)buffer;
    if(len < DELIM_LEN){
        DBG("Failed on delim\n");
        return 0;
    }

    DBG("Success on delim\n");
    return DELIM_LEN;
}


int main(int argc, char** argv)
{
    //We don't use these for the test ... yet
    (void)argc;
    (void)argv;
    char* uri = "tcp:localhost:2000?listen=1";

    //Create a new multiplexer for streams to go into
    camio_mux_t* mux = NULL;
    camio_error_t err = camio_mux_new(CAMIO_MUX_HINT_PERFORMANCE, &mux);


    //Construct a delimiter
    ch_word id;
    err = camio_transport_get_id("delim",&id);
    delim_params_t delim_params = { .base_uri = uri, .delim_fn = delimit };
    ch_word params_size = sizeof(delim_params_t);
    void* params = &delim_params;

    //Use the parameters structure to construct a new connector object
    camio_connector_t* connector = NULL;
    err = camio_transport_constr(id,&params,params_size,&connector);
    if(err){
        DBG("Could not construct connector\n");
        return CAMIO_EINVALID; //TODO XXX put a better error here
    }


    while( (err = camio_connector_ready(connector)) == CAMIO_ENOTREADY ){}
    if(err != CAMIO_EREADY){
        DBG("Unexpected error in connector\n");
        return CAMIO_EINVALID;
    }

    camio_stream_t* io_stream = NULL;
    err = camio_connect(connector,&io_stream);
    if(err){ DBG("Could not connect to stream\n"); return CAMIO_EINVALID; /*TODO XXX put a better error here*/ }

    //Put the read stream into the mux
    typedef enum { READ_STREAM, WRITE_STREAM } camio_cat_state_e;
    camio_mux_insert(mux,&io_stream->rd_muxable,READ_STREAM);
    camio_mux_insert(mux,&io_stream->wr_muxable,WRITE_STREAM);

   //Read and write bytes to and from the stream - just do loopback for now
    camio_rd_buffer_t* rd_buffer = NULL;
    camio_rd_buffer_t* wr_buffer = NULL;
    camio_muxable_t* muxable     = NULL;
    ch_word which                = -1;
    camio_read_req_t  rd_req = { 0 };
    camio_write_req_t wr_req = { 0 };
    err = camio_read_request(io_stream,&rd_req,1); //kick the process off -- tell the read stream that we would like some data,
    if(err){ DBG("Got a read request error %i\n", err); return -1; }

    while(1){
        //Block waiting for a stream to be ready
        err = camio_mux_select(mux,&muxable,&which);
        if(err != CAMIO_ENOERROR){
            DBG("Unexpected error %lli on stream with id =%lli\n", err, which);
            break;
        }
        switch(which){
            case READ_STREAM: {//There is new data to be read
                //Acquire a pointer to the new data now that it's ready
                DBG("Handling read event\n");
                err = camio_read_acquire(muxable->parent.stream, &rd_buffer);
                if(err){ DBG("Got a read error %i\n", err); return -1; }
                DBG("Got %lli bytes of new data at %p\n", rd_buffer->data_len, rd_buffer->data_start);

                if(rd_buffer->data_len == 0){
                    DBG("The connection is closed, exiting now\n");
                    return -1; //The connection is dead!
                }

                //Get a buffer for writing stuff to
                err = camio_write_acquire(io_stream, &wr_buffer);
                if(err){ DBG("Could acquire new write buffer %lli\n", err); return -1; /*TODO XXX put a better error here*/ }

                //Copy data over from the read buffer to the write buffer
                err = camio_copy_rw(&wr_buffer,&rd_buffer,0,0,rd_buffer->data_len);
                if(err){ DBG("Got a copy error %i\n", err); return -1; }

                //Data copied, let's commit it and send it off
                wr_req.buffer = wr_buffer;
                err = camio_write_request(io_stream, &wr_req,1);
                if(err){ DBG("Got a write request error %i\n", err); return -1; }

                //And we're done with the read buffer now, release it
                err = camio_read_release(io_stream, &rd_buffer);
                if(err){ DBG("Got a read release error %i\n", err); return -1; }
                break;
            }
            case WRITE_STREAM: {//Data has finished writing
                DBG("Handling write event\n");
                //Done with the write buffer, release it
                err = camio_write_release(io_stream,&wr_buffer);
                if(err){ DBG("Got a write release error %i\n", err); return -1; }

                DBG("req.off=%lli req.dst=%lli req.size=%lli\n",rd_req.src_offset_hint, rd_req.dst_offset_hint, rd_req.read_size_hint);
                err = camio_read_request(io_stream,&rd_req,1); //Tell the read stream that we would like some more data..
                if(err){ DBG("Got a read request error %i\n", err); return -1; }
                break;
            }
        }
    }

    return 0;
}
