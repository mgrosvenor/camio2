/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Jul 20, 2014
 *  File name: errors.h
 *  Description:
 *  Error codes and error numbers that are global to CamIO
 */

#ifndef ERRORS_H_
#define ERRORS_H_
//TODO XXX: write a string translation table for displaying the errors nicely

typedef enum camio_error_e {
    CAMIO_NOTIMPLEMENTED = -1,
    CAMIO_ENOERROR = 0,
    CAMIO_ETRYAGAIN,
    CAMIO_ECHECKERRORNO,
    CAMIO_ENOSLOTS,
    CAMIO_EQFU,
    CAMIO_ECOPYOP,
    CAMIO_EBADURI,
    CAMIO_ENOSTREAM,
    CAMIO_EBADOPT,
    CAMIO_EBADPROP,
    CAMIO_EUNSUPSTRAT,
    CAMIO_EUNSUPMODE,
    CAMIO_EINVALID,
    CAMIO_ENOMEM,
    CAMIO_ENOSCHEME,
    CAMIO_ENOHIER,
    CAMIO_ENOKEY,
    CAMIO_EUNKNOWNSTATE,
    CAMIO_EURIOPTEXITS,
    CAMIO_EURIOPTUNKNOWNTYPE,
    CAMIO_EURIOPTBADTYPE,
    CHI_EURIOPTNOTFLAG,
    CHI_EURIOPTREQUIRED,
    CAMIO_EFILEIOEXCLUSIVERW,
    CAMIO_ETOOMANYFLAGS,
    CAMIO_NOTSELECTABLE,
    CAMIO_SELECTORFU,
    CAMIO_EINDEXNOTFOUND,
    CAMIO_EEMPTY,
    CAMIO_EALLREADYCONNECTED,
} camio_error_t;

#endif /* ERRORS_H_ */
