/*
 * selector.h
 *
 *  Created on: Oct 18, 2013
 *      Author: mgrosvenor
 */

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



/**
 * All streams in CamIO are non-blocking. Blocking behaviour is achieved through using a selector. Both read and write
 * streams may be placed into a selector. Selection strategies vary and some streams may implement many. A selector is given
 * a string based target optimisation criteria or all streams can be forced to use a single selection strategy. In this
 * case, streams that do not support the selection strategy will be rejected.
 */
ciosel* new_selector(char* strategy);


/**
 * Insert a new stream into the selector called “this”. The id will be returned by the selector when an event matching the
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
int ciosel_insert(ciosel* this, cioselable* selectable,  cioselmode mode, int id);


/**
 * Remove the stream with the given id from the selector.
 */
void ciosel_remove(ciosel* this, int id);

/**
 * Return the ID and pointer to the selectable object when it becomes ready.
 */
int ciosel_select(ciosel* this, cioselable** selectable_o);



#endif /* SELECTOR_H_ */
