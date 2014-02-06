// CamIO 2: ciostreams_fileio.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#include "ciostream_fileio.h"
#include "../types/stdinclude.h"

//Keep this definition in the C file, so it can't escape.
//This is private!
typedef struct {
    char* filename;
    int flags;
} private;


static cioselable* get_selectable(ciostr* this)
{
    (void)this;
    return NULL;
}


static int connect( ciostr* this, struct ciostrm_s* ciostrm_o )
{
    (void)this;
    (void)ciostrm_o;
    return -1;
}

static void destroy(ciostr* this)
{
    (void)this;
}



//Make a new fileio connector
int fileio_new_cioconn( uri* uri_parsed , struct cioconn_s** cioconn_o, void** global_data )
{
    //Make a new connector
    cioconn* connector = calloc(0,sizeof(cioconn));
    if(!connector){
        return CIO_ENOMEM;
    }

    //Populate it
    connector->connect          = connect;
    connector->get_selectable   = get_selectable;
    connector->destroy          = destroy;

    //Make a new private structure
    connector->__priv = calloc(0,sizeof(private));
    if(!connector->__priv){
        return CIO_ENOMEM;
    }

    //Populate it
    private* priv   = connector->__priv;
    priv->filename  = NULL;
    priv->flags     = 0;


    //This stream has no global data ... yet?
    (void)global_data;

    //Output the connector
    *cioconn_o = connector;

    //Done!
    return CIO_ENOERROR;
}
