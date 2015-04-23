/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 17, 2014
 *  File name: selectable.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#ifndef SELECTABLE_H_
#define SELECTABLE_H_

#include "../types/types.h"

/**
 * The interface implemented by all objects that are selectable
 */

struct camio_selectable_s {
    int (*ready)(camio_selectable_t* this); //Returns non-zero if a call to start_write will be non-blocking
    int fd; //This may be -1 if there is no file descriptor

} ;

#endif /* SELECTABLE_H_ */
