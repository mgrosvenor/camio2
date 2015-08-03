// CamIO 2: camio2.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk)
// Licensed under BSD 3 Clause, please see LICENSE for more details.


#include <stdio.h>
#include "ciochannels/ciochannel.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("Hello  world\n");
    #if NDEBUG
    printf("Release Mode on!!\n");
    #endif

    ciodev* con;
    new_ciodev("file:/tmp/myfile?ro,trunc",NULL,&con);
    con->devect(con);

    return 0;
}

