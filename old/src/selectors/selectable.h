/*
 * selectable.h
 *
 *  Created on: Oct 18, 2013
 *      Author: mgrosvenor
 */

#ifndef SELECTABLE_H_
#define SELECTABLE_H_

/**
 * The interface implemented by all objects that are selectable
 */
struct cioselable_s;
typedef struct cioselable_s cioselable;

struct cioselable_s {
    int (*ready)(cioselable* this); //Returns non-zero if a call to start_write will be non-blocking
    int fd; //This may be -1 if there is no file descriptor

    //Per instance private
    void* __priv;
};

#endif /* SELECTABLE_H_ */
