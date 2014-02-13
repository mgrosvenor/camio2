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



size_t camio_selector_spin_count(ciosel* this){
    selector_spin_priv* priv = this->__priv;
    return priv->stream_count;
}


//Remove the istream at index specified
int camio_selector_spin_remove(ciosel* this, size_t index){
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

//Block waiting for a change on a given istream
//return the stream number that changed
uint64_t str0= 0;
uint64_t str1 = 0;
size_t camio_selector_spin_select(camio_selector_t* this){
    camio_selector_spin_t* priv = this->priv;

    size_t i = (priv->last + 1) % priv->stream_count; //Start from the next stream to avoid starvation
    while(1){
        for(; i < priv->stream_count; i++){
            if(likely(priv->streams[i].stream != NULL)){
                if(likely(priv->streams[i].stream->ready(priv->streams[i].stream))){
                    priv->last = i;
                    //printf("[0x%016lx] selector fired at index %lu\n", priv->streams[i].index, i);
                    return priv->streams[i].index;
                }
            }
 
        }
    }
    return ~0; //Unreachable
}


void camio_selector_spin_delete(camio_selector_t* this){
    camio_selector_spin_t* priv = this->priv;
    free(priv);
}

/* ****************************************************
 * Construction
 */

camio_selector_t* camio_selector_spin_construct(camio_selector_spin_t* priv, camio_selector_spin_params_t* params){
    if(!priv){
        eprintf_exit("spin stream supplied is null\n");
    }
    //Initialize the local variables
    priv->params           = params;
    priv->stream_count     = 0;
    priv->last			   = 0;
    bzero(&priv->streams,sizeof(camio_selector_spin_stream_t) * CAMIO_SELECTOR_SPIN_MAX_STREAMS) ;



    //Populate the function members
    priv->selector.priv          = priv; //Lets us access private members
    priv->selector.init          = camio_selector_spin_init;
    priv->selector.insert        = camio_selector_spin_insert;
    priv->selector.remove        = camio_selector_spin_remove;
    priv->selector.select        = camio_selector_spin_select;
    priv->selector.delete        = camio_selector_spin_delete;
    priv->selector.count         = camio_selector_spin_count;

    //Call init, because its the obvious thing to do now...
    priv->selector.init(&priv->selector);

    //Return the generic selector interface for the outside world to use
    return &priv->selector;

}

ciosel* selector_spin_new(){
    ciosel* selector = malloc(sizeof(ciosel));
    if(!selector){
        return NULL;
    }

    selector_spin_priv* priv = malloc(sizeof(selector_spin_priv));
    if(!priv){
        free(selector);
        return NULL;
    }

    selector->__priv = priv;






}





