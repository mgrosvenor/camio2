// CamIO 2: ciochannel_fileio.h
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#ifndef CIOSTREAM_FILEIO_H_
#define CIOSTREAM_FILEIO_H_

#include "../uri_parser/uri_parser.h"
#include "ciochannel.h"
#include "../types/types.h"

int new_cioconn_fileio( uri* uri_parsed , struct cioconn_s** cioconn_o, void** global_data );


#endif /* CIOSTREAM_FILEIO_H_ */
