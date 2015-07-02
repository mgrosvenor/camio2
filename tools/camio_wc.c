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


//A simple new line delimiter
ch_word delimit(char* buffer, ch_word len)
{
    DBG("Running delimiter on buffer=%p with len=%i\n", buffer,len);

    const char* result = memchr(buffer,'\n',len);
    if(!result){
        DBG("Failed, could not find a new line\n");
        return -1;
    }

    const ch_word offset = result - (char*)buffer + 1;
    DBG("Success result =%p, offset=%lli\n", result,offset);
    DBG("--> %.*s\n", (int)offset - 1, buffer);
    return offset;

}

typedef enum { READ_STREAM } camio_cat_state_e;

int main(int argc, char** argv)
{
    //We don't use these for the test ... yet
    if(argc < 2){
        printf("usage camio_wc <URI>\n");
        return -1;
    }
    char* uri = argv[1];

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

    //Spin waiting for a connection. A little bit naughty.
    while( (err = camio_connector_ready(connector)) == CAMIO_ENOTREADY ){}
    if(err != CAMIO_EREADY){
        DBG("Unexpected error in connector\n");
        return CAMIO_EINVALID;
    }

    camio_stream_t* io_stream = NULL;
    err = camio_connect(connector,&io_stream);
    if(err){ DBG("Could not connect to stream\n"); return CAMIO_EINVALID; /*TODO XXX put a better error here*/ }

    //Put the read stream into the mux
    camio_mux_insert(mux,&io_stream->rd_muxable,READ_STREAM);

   //Read and write bytes to and from the stream - just do loopback for now
    camio_rd_buffer_t* rd_buffer = NULL;
    camio_muxable_t* muxable     = NULL;
    ch_word which                = -1;
    camio_read_req_t  rd_req = {
            .src_offset_hint = CAMIO_READ_REQ_SRC_OFFSET_NONE,
            .dst_offset_hint = CAMIO_READ_REQ_DST_OFFSET_NONE,
            .read_size_hint  = CAMIO_READ_REQ_SIZE_ANY
    };
    err = camio_read_request(io_stream,&rd_req,1); //kick the process off -- tell the read stream that we would like some data,
    if(err){ DBG("Got a read request error %i\n", err); return -1; }

    ch_word line_count = 0;

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
                    printf("Found %lli lines\n", line_count);
                    return -1; //The connection is dead!
                }

                DBG("Incrementing line count - currently %lli\n", line_count);
                line_count++;

                //And we're done with the read buffer now, release it
                err = camio_read_release(io_stream, &rd_buffer);
                if(err){ DBG("Got a read release error %i\n", err); return -1; }

                err = camio_read_request(io_stream,&rd_req,1); //kick the process off -- tell the read stream that we would like some data,
                if(err){ DBG("Got a read request error %i\n", err); return -1; }
                break;
            }
            default:{
                DBG("Eeek. Something strange happened\n");
                return -1;
            }

        }
    }

    return 0;
}
