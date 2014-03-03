/*
 * errors.h
 *
 *  Created on: Oct 18, 2013
 *      Author: mgrosvenor
 */

#ifndef ERRORS_H_
#define ERRORS_H_

//TODO XXX: Move to using an enum, write a string translation table for displaying the errors nicely

#define CIO_ENOERROR              0LL
#define CIO_ETRYAGAIN            -1LL
#define CIO_ECHECKERROR          -2LL
#define CIO_ENOSLOTS             -3LL
#define CIO_EQFULL               -4LL
#define CIO_ECOPYOP              -5LL
#define CIO_EBADURI              -6LL
#define CIO_ENOSTREAM            -7LL
#define CIO_EBADOPT              -8LL
#define CIO_EBADPROP             -9LL
#define CIO_EUNSUPSTRAT         -10LL
#define CIO_EUNSUPMODE          -11LL
#define CIO_EINVALID            -12LL
#define CIO_ENOMEM              -13LL
#define CIO_ENOSCHEME           -14LL
#define CIO_ENOHIER             -15LL
#define CIO_ENOKEY              -16LL
#define CIO_EUNKNOWNSTATE       -17LL
#define CIO_EURIOPTEXITS        -18LL
#define CIO_EURIOPTUNKNOWNTYPE  -19LL
#define CIO_EURIOPTBADTYPE      -20LL
#define CHI_EURIOPTNOTFLAG      -21LL
#define CHI_EURIOPTREQUIRED     -22LL
#define CIO_EFILEIOEXCLUSIVERW  -23LL
#define CIO_ETOOMANYFLAGS       -24LL
#define CIO_NOTSELECTABLE       -25LL
#define CIO_SELECTORFULL        -26LL
#define CIO_EINDEXNOTFOUND      -27LL

#endif /* ERRORS_H_ */
