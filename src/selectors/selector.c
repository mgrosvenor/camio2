// CamIO 2: selector.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details.

#include <string.h>

#include "../errors/errors.h"

#include "selector.h"
#include "selector_spin.h"



ciosel* new_selector(char* strategy){
    ciosel* result = NULL;

    if(strcmp(strategy,"spin") == 0 ){
        result = selector_spin_new();
    }

    return result;
}
