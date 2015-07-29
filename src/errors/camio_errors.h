/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 2014
 *  File name: camio_errors.h
 *  Description:
 *  Error codes and error numbers that are global to CamIO
 */

#ifndef CAMIO_ERRORS_H_
#define CAMIO_ERRORS_H_
//TODO XXX: write a string translation table for displaying the errors nicely

typedef enum camio_error_e {
    CAMIO_NOTIMPLEMENTED = -1,
    CAMIO_ENOERROR = 0,         //This must be 0!
    CAMIO_ENOBUFFS,             //1
    CAMIO_ETRYAGAIN,            //2
    CAMIO_ECHECKERRORNO,        //3
    CAMIO_ENOSLOTS,             //4
    CAMIO_EQFU,                 //5
    CAMIO_ECOPYOP,              //6
    CAMIO_EBADURI,              //7
    CAMIO_ENOCHANNEL,            //8
    CAMIO_EBADOPT,              //9
    CAMIO_EBUFFINUSE,           //10
    CAMIO_EBUFFERROR,           //11
    CAMIO_EBADPROP,             //12
    CAMIO_EUNSUPSTRAT,          //13
    CAMIO_EUNSUPMODE,           //14
    CAMIO_EINVALID,             //15
    CAMIO_EWRONGBUFF,           //16
    CAMIO_ENOMEM,               //17
    CAMIO_ENOSCHEME,            //18
    CAMIO_ENOHIER,              //19
    CAMIO_ENOKEY,               //20
    CAMIO_EUNKNOWNSTATE,        //21
    CAMIO_EURIOPTEXITS,         //22
    CAMIO_EURIOPTUNKNOWNTYPE,   //23
    CAMIO_EURIOPTBADTYPE,       //24
    CAMIO_EFILEIOEXCLUSIVERW,   //25
    CAMIO_ETOOMANYFLAGS,        //26
    CAMIO_NOTSELECTABLE,        //27
    CAMIO_SELECTORFU,           //28
    CAMIO_EINDEXNOTFOUND,       //29
    CAMIO_EEMPTY,               //30
    CAMIO_EALLREADYCONNECTED,   //31
    CAMIO_ENOTREADY,            //32
    CAMIO_EREADY,               //33
    CAMIO_ETOOMANY,             //34
    CAMIO_ECLOSED,              //35
    CAMIO_ENOMORE,              //36
    CAMIO_ENOREQS,              //37
    CAMIO_ERELBUFF,             //38
    CAMIO_ERRMUXTIMEOUT,        //39
} camio_error_t;

#endif /* CAMIO_ERRORS_H_ */
