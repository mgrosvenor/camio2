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
//TODO XXX: Move to using an enum, write a string translation table for displaying the errors nicely

typedef enum camio_error_e {
    CIO_ENOERROR,
    CIO_ETRYAGAIN,
    CIO_ECHECKERROR,
    CIO_ENOSLOTS,
    CIO_EQFU,
    CIO_ECOPYOP,
    CIO_EBADURI,
    CIO_ENOSTREAM,
    CIO_EBADOPT,
    CIO_EBADPROP,
    CIO_EUNSUPSTRAT,
    CIO_EUNSUPMODE,
    CIO_EINVALID,
    CIO_ENOMEM,
    CIO_ENOSCHEME,
    CIO_ENOHIER,
    CIO_ENOKEY,
    CIO_EUNKNOWNSTATE,
    CIO_EURIOPTEXITS,
    CIO_EURIOPTUNKNOWNTYPE,
    CIO_EURIOPTBADTYPE,
    CHI_EURIOPTNOTFLAG,
    CHI_EURIOPTREQUIRED,
    CIO_EFILEIOEXCLUSIVERW,
    CIO_ETOOMANYFLAGS,
    CIO_NOTSELECTABLE,
    CIO_SELECTORFU,
    CIO_EINDEXNOTFOUND,
    CIO_EEMPTY,
} camio_error_t;

#endif /* ERRORS_H_ */
