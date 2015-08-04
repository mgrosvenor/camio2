/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jul 28, 2015
 *  File name:  messages.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#ifndef MESSAGES_H_
#define MESSAGES_H_

typedef struct camio_read_buff_req_s {
    camio_buffer_t* _;          //This is a placeholder - the order is important
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
    ch_word read_size_hint;
} camio_rd_buff_req_t;

typedef struct camio_read_buff_res_s {
    camio_buffer_t* buffer ; //This will be populated if the return code is CAMIO_ENOERROR -- the order is important
    camio_error_t status;    //Return code indicating what happened to the request.
} camio_rd_buff_res_t;


#define CAMIO_READ_REQ_SIZE_ANY (-1)
#define CAMIO_READ_REQ_SRC_OFFSET_NONE (-1)
#define CAMIO_READ_REQ_DST_OFFSET_NONE (0)
typedef struct camio_read_data_req_s {
    camio_buffer_t* buffer;     //-- the order is important
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
    ch_word read_size_hint;
} camio_rd_data_req_t;

typedef struct camio_read_data_res_s {
    camio_buffer_t* buffer;     //This will be populated if the return code is CAMIO_ENOERROR -- the order is important
    camio_error_t status;       //Return code indicating what happened to the request.
} camio_rd_data_res_t;


typedef struct camio_write_buff_req_s {
    camio_buffer_t* _;          //This is a placeholder - the order is important!!
} camio_wr_buff_req_t;

typedef struct camio_write_buff_res_s {
    camio_buffer_t* buffer;  //This will be populated if the return code is CAMIO_ENOERROR -- the order is important
    camio_error_t status;    //Return code indicating what happened to the request.
} camio_wr_buff_res_t;


#define CAMIO_WRITE_REQ_DST_OFFSET_NONE (-1)
#define CAMIO_WRITE_REQ_SRC_OFFSET_NONE (0)
typedef struct camio_write_data_req_s {
    camio_buffer_t* buffer; //-- The order is important
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
} camio_wr_data_req_t;


typedef struct camio_write_data_res_s {
    camio_buffer_t* buffer;   //This will be populated if the return code is CAMIO_ENOERROR -- the order is important
    camio_error_t status;     //Return code indicating what happened to the request.
    ch_word written;
} camio_wr_data_res_t;


typedef struct camio_chan_req_s {
    camio_channel_t* _;     //This is a place holder -- the order is important!!
} camio_chan_req_t;

typedef struct camio_chan_res_s {
    camio_channel_t* channel; //-- the order is important
    ch_word id;
    camio_error_t status;
} camio_chan_res_t;


typedef struct camio_msg_error_s {
    camio_error_t status;     //Return code indicating what happened to the request.
} camio_msg_error_t;


typedef enum camio_msg_type_en {
    CAMIO_MSG_TYPE_NONE = 0,            //0  -- initial state, no message here
    CAMIO_MSG_TYPE_CHAN_REQ,            //1  -- request a channel from a CamIO device
    CAMIO_MSG_TYPE_CHAN_REQ_QD,         //2  -- the request has been queued into a request queue, but no response yet
    CAMIO_MSG_TYPE_CHAN_RES,            //3  -- response to a channel request
    CAMIO_MSG_TYPE_READ_BUFF_REQ,       //4  -- request a buffer for reading into
    CAMIO_MSG_TYPE_READ_BUFF_REQ_QD,    //5  -- request for a buffer has been queued, but no response yet
    CAMIO_MSG_TYPE_READ_BUFF_RES,       //6  -- response to a read buffer request
    CAMIO_MSG_TYPE_READ_DATA_REQ,       //7  -- request that data is made available in the read buffer
    CAMIO_MSG_TYPE_READ_DATA_REQ_QD,    //8  -- request for data has been queued, but no response yet
    CAMIO_MSG_TYPE_READ_DATA_RES,       //9  -- response to a read data request
    CAMIO_MSG_TYPE_WRITE_BUFF_REQ,      //10 -- request a buffer for writing into
    CAMIO_MSG_TYPE_WRITE_BUFF_REQ_QD,   //11 -- request for a buffer has been queued, but no response yet
    CAMIO_MSG_TYPE_WRITE_BUFF_RES,      //12 -- response to a write buffer request
    CAMIO_MSG_TYPE_WRITE_DATA_REQ,      //13 -- request that data is made available in the write buffer
    CAMIO_MSG_TYPE_WRITE_DATA_REQ_QD,   //14 -- request for data has been queued, but no response yet
    CAMIO_MSG_TYPE_WRITE_DATA_RES,      //15 -- response to a write data request
    CAMIO_MSG_TYPE_IGNORE,              //16 -- any CamIO function that sees this message should simply skip it
    CAMIO_MSG_TYPE_ERROR,               //17 -- An error has occurred, you should check the status value
} camio_msg_type_e;


typedef struct camio_msg_s {
    camio_msg_type_e type;  //What type of message is this?
    ch_word id;             //User field to keep track of requests and responses. This is not altered by CamIO.
    union {
        camio_chan_req_t ch_req;
        camio_chan_res_t ch_res;
        camio_rd_buff_req_t rd_buff_req;
        camio_rd_buff_res_t rd_buff_res;
        camio_rd_data_req_t rd_data_req;
        camio_rd_data_res_t rd_data_res;
        camio_wr_buff_req_t wr_buff_req;
        camio_wr_buff_res_t wr_buff_res;
        camio_wr_data_req_t wr_data_req;
        camio_wr_data_res_t wr_data_res;
        camio_msg_error_t   err_res;
    };
} camio_msg_t;


#endif /* MESSAGES_H_ */
