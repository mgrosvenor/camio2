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
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
    ch_word read_size_hint;
} camio_rd_buff_req_t;

typedef struct camio_read_buff_res_s {
    camio_error_t status;       //Return code indicating what happened to the request.
    camio_buffer_t* buffer;  //This will be populated if the return code is CAMIO_ENOERROR
} camio_rd_buff_res_t;


#define CAMIO_READ_REQ_SIZE_ANY (-1)
#define CAMIO_READ_REQ_SRC_OFFSET_NONE (-1)
#define CAMIO_READ_REQ_DST_OFFSET_NONE (0)
typedef struct camio_read_data_req_s {
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
    ch_word read_size_hint;
    camio_buffer_t* buffer;
} camio_rd_data_req_t;

typedef struct camio_read_data_res_s {
    camio_error_t status;       //Return code indicating what happened to the request.
    camio_buffer_t* buffer;  //This will be populated if the return code is CAMIO_ENOERROR
} camio_rd_data_res_t;


typedef struct camio_write_buff_req_s {
    ch_word _;                  //C11 does not permit empty structures
} camio_wr_buff_req_t;

typedef struct camio_write_buff_res_s {
    camio_error_t status;       //Return code indicating what happened to the request.
    camio_buffer_t* buffer;  //This will be populated if the return code is CAMIO_ENOERROR
} camio_wr_buff_res_t;


#define CAMIO_WRITE_REQ_DST_OFFSET_NONE (-1)
#define CAMIO_WRITE_REQ_SRC_OFFSET_NONE (0)
typedef struct camio_write_data_req_s {
    ch_word src_offset_hint;
    ch_word dst_offset_hint;
    camio_buffer_t* buffer;
} camio_wr_data_req_t;


typedef struct camio_write_data_res_s {
    camio_error_t status;       //Return code indicating what happened to the request.
    camio_buffer_t* buffer;  //This will be populated if the return code is CAMIO_ENOERROR
} camio_wr_data_res_t;


typedef struct camio_chan_req_s {
    ch_word _; //C11 does not support empty structures
} camio_chan_req_t;

typedef struct camio_chan_res_s {
    camio_channel_t* channel;
    ch_word id;
    ch_word status;
} camio_chan_res_t;

typedef enum camio_msg_type_en {
    CAMIO_MSG_TYPE_NONE,
    CAMIO_MSG_TYPE_CHAN_REQ,
    CAMIO_MSG_TYPE_CHAN_RES,
    CAMIO_MSG_TYPE_READ_BUFF_REQ,
    CAMIO_MSG_TYPE_READ_BUFF_RES,
    CAMIO_MSG_TYPE_READ_DATA_REQ,
    CAMIO_MSG_TYPE_READ_DATA_RES,
    CAMIO_MSG_TYPE_WRITE_BUFF_REQ,
    CAMIO_MSG_TYPE_WRITE_BUFF_RES,
    CAMIO_MSG_TYPE_WRITE_DATA_REQ,
    CAMIO_MSG_TYPE_WRITE_DATA_RES,
    CAMIO_MSG_TYPE_IGNORE,
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
    };
} camio_msg_t;


#endif /* MESSAGES_H_ */
