/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 16, 2015
 *  File name:  spin_mux.c
 *  Description:
 *  A very simple spinning multiplexer. This will work ok so long as you don't mind burning a CPU and the rate at which new
 *  transports come and go is low.
 */

#include "mux.h"
#include <src/types/muxable_vec.h>
#include <src/camio_debug.h>

typedef struct {
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables;
    ch_word idx;
} mux_spin_priv_t;

camio_error_t spin_construct(camio_mux_t* this){
    DBG("Making new vector spin mux\n");

    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    priv->muxables = CH_VECTOR_NEW(CAMIO_MUXABLE_VEC,1024,CH_VECTOR_CMP(CAMIO_MUXABLE_VEC));
    if(!priv->muxables){
        return CAMIO_ENOMEM;
    }

    priv->idx = 0;
    return CAMIO_ENOERROR;
}


camio_error_t spin_insert(camio_mux_t* this, camio_muxable_t* muxable, ch_word id)
{
    DBG("Inserting %p into mux with id=%i\n",muxable,id);
    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables = priv->muxables;
    muxable->id = id;
    muxables->push_back(muxables, *muxable);
    return CAMIO_ENOERROR;
}


camio_error_t spin_remove(camio_mux_t* this, camio_muxable_t* muxable)
{
    DBG("Removing %p from mux\n");
    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables = priv->muxables;
    camio_muxable_t* result = muxables->find(muxables, muxables->first, muxables->end, *muxable);
    if(!result){
        return CAMIO_EINVALID; //TODO XXX figure out better names for this
    }
    muxables->remove(muxables, result); //This may be a little expensive depending on the rate.

    return CAMIO_NOTIMPLEMENTED;
}


camio_error_t spin_select(camio_mux_t* this, /*struct timespec timeout,*/ camio_muxable_t** muxable_o, ch_word* which_o)
{
   /* (void)timeout; //Ignore the timeouts for the moment TODO - Implement this! */

    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables = priv->muxables;
    DBG("Selecting over %i items\n", muxables->count);

    while(1){
        //priv->idx %= muxables->count; //Adjust for overflow
        camio_muxable_t* muxable = muxables->off(muxables,priv->idx);
        camio_error_t err = muxable->vtable.ready(muxable);
        if(err == CAMIO_EREADY){
            DBG("Found ready item at index %i\n", priv->idx);
            priv->idx = priv->idx >= muxables->count - 1 ? 0 : priv->idx + 1;//Make sure we look at the next transport first
            *muxable_o = muxable;
            *which_o = muxable->id;
            return CAMIO_ENOERROR;
        }

        if(err != CAMIO_ENOTREADY){
            DBG("Muxable had an unexpected error = %i\n", err);
            *muxable_o = muxable;
            *which_o = muxable->id;
            return err;
        }

        priv->idx = priv->idx >= muxables->count - 1 ? 0 : priv->idx + 1;

    }

    //Unreachable
    return CAMIO_EINVALID; //TODO BETTER ERROR CODE HERE!
}

void spin_destroy(camio_mux_t* this)
{
    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    if(priv->muxables){
        CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables = priv->muxables;
        muxables->delete(muxables);
    }
    free(this);
    return;
}


NEW_MUX_DEFINE(spin,mux_spin_priv_t)
