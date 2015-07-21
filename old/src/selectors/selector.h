// CamIO 2: selector.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details.
#ifndef SELECTOR_H_
#define SELECTOR_H_

//CamIO IO selector definition
struct ciosel_s;
typedef struct ciosel_s ciosel;

typedef enum {
    CIOSEL_MODE_READ,
    CIOSEL_MODE_WRITE,
    CIOSEL_MODE_CON,
    CIOSEL_MOD_TIMEO
} cioselmode;


struct ciosel_s {
    /**
     * Insert a new channel into the selector called “this”. The id will be returned by the selector when an event matching the
     * mode happens. Mode may be either:
     * - CIOSEL_MODE_READ - Only wake up when new data is ready to ready
     * - CIOSEL_MODE_WRITE - Only wake up when data can be written
     * - CIOSEL_MODE_CON - Wake up on a successful connection signal
     * - CIOSEL_MODE_TIMEO - Wake up when a timeout happens
     *
     * Return values;
     * - ENOERROR: Selector insertion was successful.
     * - EUNSUPSTRAT: Unsupported selection strategy is requested.
     * - EUNSUPMODE: Unsupported selection mode is requested.
     */
    int (*insert)(ciosel* this, cioselable* selectable,  cioselmode mode, int id);


    /**
     * Remove the channel with the given id from the selector.
     */
    void (*remove)(ciosel* this, int id);

    /**
     * Return the ID and pointer to the selectable object when it becomes ready.
     */
    int (*select)(ciosel* this, cioselable** selectable_o);

    /*
     * Returns the number of channels in this selctor
     */
    size_t (*count)(ciosel* this);

    /*
     * Free any resources associated with this selector
     */
    size_t (*destroy)(ciosel* this);


    /**
     * Per-selector data
     */
    void* __priv;
};

/**
 * All channels in CamIO are non-blocking. Blocking behaviour is achieved through using a selector. Both read and write
 * channels may be placed into a selector. Selection strategies vary and some channels may implement many. A selector is given
 * a string based target optimisation criteria or all channels can be forced to use a single selection strategy. In this
 * case, channels that do not support the selection strategy will be rejected.
 */
typedef enum { LOW_LATENCY, LOW_CPU, __SPIN, __POLL,  __EPOLL,  __SELECT } ciosel_strategy;

ciosel* new_selector(ciosel_strategy strategy);


#endif /* SELECTOR_H_ */
