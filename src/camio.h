/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: camio.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef CAMIO_H_
#define CAMIO_H_

#include <api/api.h>

//Container for all CamIO state
typedef struct camio_s {
    int some_state;
} camio_t;

extern camio_t __camio_state_container;

#define USE_CAMIO camio_t __camio_state_container

camio_t* init_camio();



#endif /* CAMIO_H_ */
