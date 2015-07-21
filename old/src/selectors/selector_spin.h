// CamIO 2: selector_spin.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details.

#ifndef SELECTOR_SPIN_H_
#define SELECTOR_SPIN_H_

#include "selector.h"
#include "selectable.h"

/********************************************************************
 *                  PRIVATE DEFS
 ********************************************************************/


typedef struct {
    cioselable* selectable;
    size_t index;
} selector_spin_item_t;

#define SELECTOR_SPIN_MAX_ITEMS 4096

typedef struct {
    //XXX TODO: Convert to using chaste vector type for more flexibility and power

    //Statically allow up to n channels on this (simple) selector
    selector_spin_item_t items[SELECTOR_SPIN_MAX_ITEMS];
    //Number of channels added to the selector
    int64_t channel_count;
    int64_t last;
} selector_spin_priv;



/********************************************************************
 *                  PUBLIC DEFS
 ********************************************************************/

ciosel* selector_spin_new();


#endif /* SELECTOR_SPIN_H_ */
