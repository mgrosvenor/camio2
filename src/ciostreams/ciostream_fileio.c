// CamIO 2: ciostreams_fileio.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#include "ciostream_fileio.h"
#include "../types/stdinclude.h"
#include "../uri_parser/uri_opts.h"
#include "../selectors/selectable.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

//Keep this definition in the C file, so it can't escape.
//This is private!
typedef struct {
    char* filename;
    int flags;
    int mode;
    int fd;
    cioselable selectable;
} private;


static cioselable* get_selectable(cioconn* this)
{
    (void)this;
    return NULL;
}


static int connect( cioconn* this, ciostrm* ciostrm_o )
{
    private* priv = (private*)this->__priv;
    priv->fd = open(priv->filename,priv->flags);


    (void)ciostrm_o;
    return -1;
}

static void destroy(cioconn* this)
{
    if(!this){
        return;
    }

    private* priv = (private*)this->__priv;
    if(priv){
        free(priv);
    }

    free(this);
}



//Make a new fileio connector
int new_cioconn_fileio( uri* uri_parsed , struct cioconn_s** cioconn_o, void** global_data )
{
    int result = CIO_ENOERROR;

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
    priv->mode      = 0;


    //Deal with parameters
    priv->filename = (char*)calloc(1,uri_parsed->hierarchical_len + 1); //+1 for the null terminiator
    if(!priv->filename){
        return CIO_ENOMEM;
    }
    memcpy(priv->filename, uri_parsed->hierarchical, uri_parsed->hierarchical_len);

    bool flags_readonly  = 0;
    bool flags_writeonly = 0;
    bool flags_readwrite = 0;

    //Parse the parameters
    uri_opt_parser* uop = NULL;
    if( (result = uri_opt_parser_new(&uop)) ){
        return result;
    }

    //Lots of synomyms for the same thing
    uri_opt_parser_add(uop, "RO", URIOPT_FLAG, CH_BOOL,  &flags_readonly);
    uri_opt_parser_add(uop, "WO", URIOPT_FLAG, CH_BOOL,  &flags_writeonly);
    uri_opt_parser_add(uop, "RW", URIOPT_FLAG, CH_BOOL,  &flags_readwrite);
    uri_opt_parser_add(uop, "W", URIOPT_FLAG, CH_BOOL,  &flags_writeonly);
    uri_opt_parser_add(uop, "R", URIOPT_FLAG, CH_BOOL,  &flags_readonly);
    uri_opt_parser_add(uop, "ro", URIOPT_FLAG, CH_BOOL,  &flags_readonly);
    uri_opt_parser_add(uop, "wo", URIOPT_FLAG, CH_BOOL,  &flags_writeonly);
    uri_opt_parser_add(uop, "rw", URIOPT_FLAG, CH_BOOL,  &flags_readwrite);
    uri_opt_parser_add(uop, "r", URIOPT_FLAG, CH_BOOL,  &flags_readonly);
    uri_opt_parser_add(uop, "w", URIOPT_FLAG, CH_BOOL,  &flags_writeonly);

    uri_opt_parser_parse(uop,uri_parsed);
    uri_opt_parser_free(&uop);

    //Check that only one flag is set
    bool only_one_flag = (flags_readonly ^  flags_writeonly ^  flags_readwrite) &&
                        !(flags_readonly && flags_writeonly && flags_readwrite);

    if(!only_one_flag){
        return CIO_ETOOMANYFLAGS;
    }


    priv->flags = O_RDONLY; //Default is readonly
    priv->flags = flags_writeonly ? O_WRONLY : priv->flags;
    priv->flags = flags_readwrite ? O_RDWR   : priv->flags;
    priv->flags = flags_readonly  ? O_RDONLY : priv->flags;

    //This stream has no global data ... yet?
    (void)global_data;

    //Set up the selector



    //Output the connector
    *cioconn_o = connector;

    //Done!
    return CIO_ENOERROR;
}
