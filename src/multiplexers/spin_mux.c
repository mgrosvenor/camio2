/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:    Jun 16, 2015
 *  File name:  spin_mux.c
 *  Description:
 *  A very simple spinning multiplexer. This will work ok so long as you don't mind burning a CPU and the rate at which new
 *  devices come and go is low.
 */

#include "mux.h"
#include <src/types/muxable_vec.h>
#include <sys/time.h>
#include <deps/chaste/utils/debug.h>

typedef struct {
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables;
    ch_word idx;
} mux_spin_priv_t;

camio_error_t spin_construct(camio_mux_t* this){
    //DBG("Making new vector spin mux\n");

    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    priv->muxables = CH_VECTOR_NEW(CAMIO_MUXABLE_VEC,1024,CH_VECTOR_CMP(CAMIO_MUXABLE_VEC));
    if(!priv->muxables){
        return CAMIO_ENOMEM;
    }

    priv->idx = 0;
    return CAMIO_ENOERROR;
}


camio_error_t spin_insert(camio_mux_t* this, camio_muxable_t* muxable, mux_callback_f callback, void* usr_state, ch_word id)
{
    //DBG("Inserting %p into mux with id=%i\n",muxable,id);
    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    muxable->callback       = callback;
    muxable->usr_state      = usr_state;
    muxable->id             = id;
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables = priv->muxables;
    muxables->push_back(muxables, *muxable);
    return CAMIO_ENOERROR;
}


camio_error_t spin_remove(camio_mux_t* this, camio_muxable_t* muxable)
{
    //DBG("Removing %p from mux\n");
    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables = priv->muxables;
    camio_muxable_t* result = muxables->find(muxables, muxables->first, muxables->end, *muxable);
    if(!result){
        return CAMIO_EINVALID; //TODO XXX figure out better names for this
    }
    muxables->remove(muxables, result); //This may be a little expensive depending on the rate.

    return CAMIO_NOTIMPLEMENTED;
}


camio_error_t spin_select(camio_mux_t* this, struct timeval* timeout, camio_muxable_t** muxable_o)
{

    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    CH_VECTOR(CAMIO_MUXABLE_VEC)* muxables = priv->muxables;
    //DBG("Selecting over %i items\n", muxables->count);


    //Initialize the timing. A time out of 0 returns immediately, a timeout of NULL blocks forever
    ch_word time_end    = 0;
    ch_word timeout_ns  = 0;
    ch_word time_now_ns = 0;
    struct timeval now  = {0};
    if(timeout){
        gettimeofday(&now, NULL);
        time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
        timeout_ns  = timeout->tv_sec * 1000 * 1000 * 1000 + timeout->tv_usec * 1000;
        time_end    = time_now_ns + timeout_ns;
    }

    while(1){
        //Take a look at every item once so we can exit if the timeout is 0 and nothing fires
        for(ch_word i = 0; i < muxables->count; i++,
            priv->idx = priv->idx >= muxables->count - 1 ? 0 : priv->idx + 1){

            //usleep(1000 * 731); //-- Slow things down for debugging

            if(timeout){ //Only check this if we really want the timeout
                gettimeofday(&now, NULL);
                ch_word time_now_ns = now.tv_sec * 1000 * 1000 * 1000 + now.tv_usec * 1000;
                if(time_now_ns >= time_end){
                    return CAMIO_ERRMUXTIMEOUT;
                }
            }

            //Get the next muxable and find out if it's ready
            //Not using off() here to save a few cycles in this critical loop where we know it's safe to do so
            camio_muxable_t* muxable = muxables->first + priv->idx;
            camio_error_t err        = muxable->vtable.ready(muxable);

            if(err == CAMIO_ETRYAGAIN){
                __asm__ __volatile__("pause;"); 
                continue;  //Nothing more to see here folks, come back later
            }

            //DBG("Found ready item at index %i with error code=%i\n", priv->idx, err);


            //-------------------------------------------------------------------------------------------------------------
            //Point of guaranteed return -- After this point, we will call return, so we make sure the return muxable is
            //populated and increment the idx to make sure that next time we look at the next device first. This gives some
            //degree of "fairness".

            if(muxable) {  *muxable_o  = muxable; }
            priv->idx   = priv->idx >= muxables->count - 1 ? 0 : priv->idx + 1;


            //Execute the callback if it's populated
            if(muxable->callback){
                return muxable->callback( muxable, err, muxable->usr_state, muxable->id);
            }

            return err;

        }

        //If we get here, then we have checked everything once and failed.
        if(timeout && timeout_ns == 0){
            return CAMIO_ERRMUXTIMEOUT;
        }
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


ch_word spin_count(camio_mux_t* this)
{
    mux_spin_priv_t* priv = MUX_GET_PRIVATE(this);
    return priv->muxables->count;
}


NEW_MUX_DEFINE(spin,mux_spin_priv_t)
