// CamIO 2: selector_spin.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details.

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "../errors/errors.h"
#include "selector_spin.h"


//Insert an istream at index specified
static int insert(ciosel* this, cioselable* item, size_t index){
    selector_spin_priv* priv = this->__priv;
    if(!item){
        return CIO_NOTSELECTABLE;
    }

    if(priv->stream_count >= SELECTOR_SPIN_MAX_ITEMS){
        return CIO_SELECTORFULL;
    }

    int i;
    for(i = 0; i < SELECTOR_SPIN_MAX_ITEMS; i++ ){
        if(priv->items[i].selectable == NULL){
            priv->items[i].index = index;
            priv->items[i].selectable = item;
            priv->stream_count++;
            //printf("[0x%016lx] selector added at index %i\n", priv->streams[i].index, i);
            return priv->stream_count;
        }
    }

    return 0;
}


static size_t count(ciosel* this){
    selector_spin_priv* priv = this->__priv;
    return priv->stream_count;
}


//Remove the istream at index specified
static remove(ciosel* this, size_t index){
    selector_spin_priv* priv = this->__priv;

    size_t i = 0;
    for(i = 0; i < SELECTOR_SPIN_MAX_ITEMS; i++ ){
        if(priv->items[i].index == index){
            priv->items[i].selectable = NULL;
            //printf("[0x%016lx] selector removed at index %lu\n", priv->streams[i].index, i);
            priv->stream_count--;
            return 0;
        }
    }

    if(index >= SELECTOR_SPIN_MAX_ITEMS){
        return CIO_EINDEXNOTFOUND;
    }

    //Unreachable
    return 0;
}

//Block waiting for a change on a given selectable
//return the stream index that changed
//Stream indexes are maintained by the user
static size_t select(ciosel* this){
    selector_spin_priv* priv = this->__priv;

    size_t i = (priv->last + 1) % priv->stream_count; //Start from the next stream to avoid starvation
    while(1){
        for(; i < priv->stream_count; i++){
            if(likely(priv->items[i].selectable != NULL)){
                if(likely(priv->items[i].selectable->ready(priv->items[i].selectable))){
                    priv->last = i;
                    //printf("[0x%016lx] selector fired at index %lu\n", priv->items[i].index, i);
                    return priv->items[i].index;
                }
            }
 
        }
    }
    return ~0; //Unreachable
}


static void destroy(ciosel* this){
    if(this){
        selector_spin_priv* priv = this->__priv;
        if(priv){
            free(priv);
            this->__priv = NULL;
        }

        free(this);
    }
}

/* ****************************************************
 * Construction
 */


ciosel* new_selector_spin(){


    ciosel* selector = malloc(sizeof(ciosel));
    if(!selector){
        return NULL;
    }

    selector_spin_priv* priv = malloc(sizeof(selector_spin_priv));
    if(!priv){
        free(selector);
        return NULL;
    }

    //Assign the functions
    selector->count     = count;
    selector->insert    = insert;
    selector->remove    = remove;
    selector->select    = select;
    selector->destroy   = destroy;

    //Set up private data
    selector->__priv       = priv;
    priv->stream_count     = 0;
    priv->last             = 0;
    bzero(&priv->items,sizeof(selector_spin_item_t) * SELECTOR_SPIN_MAX_ITEMS) ;



    return selector;


}





