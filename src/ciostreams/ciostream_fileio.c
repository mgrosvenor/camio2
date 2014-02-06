// CamIO 2: ciostreams_fileio.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#include "ciostream_fileio.h"
#include "../types/stdinclude.h"

int fileio_new_cioconn( uri* uri_parsed , struct cioconn_s** cioconn_o, void** global_data )
{
    printf("New file io connection\n");
    (void)uri_parsed;
    (void)cioconn_o;
    (void)global_data;
    return 0;
}
