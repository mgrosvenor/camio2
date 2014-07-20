// CamIO 2: ciostreams_fileio.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 

#include "ciostream_fileio.h"
#include "../types/stdinclude.h"
#include "../uri_parser/uri_opts.h"
#include "../selectors/selectable.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "../buffers/malloc_buffer.h"

//Keep these definitions in the C file, so they can't escape.
typedef struct {
    char* filename;
    int flags;
    int mode;
    int fd;
} conn_private;

typedef struct {
	int fd;
	malloc_buffer* buff;
	int slot_idx;
} strm_private;

/**
 * This function tries to do a non-blocking read for new data from the CamIO Stream called “this” and return slot info
 * pointer called “slot”. If the stream is empty, (e.g. end of file) or closed (e.g. disconnected) it is EEMPTU is
 * returned
 * Return values:
 * - ENOERROR:  Completed successfully, sloto contains a valid structure.
 * - ETRYAGAIN: There was no data available at this time, but there might be some more later.
 * - ENOSLOTS:  The stream could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 * - EEMPTY:	The stream has run out of data
 */
static int strm_read_acquire( ciostrm* this, cioslot* slot_o, int padding)
{
	strm_private* priv = (strm_private*)this->__priv;
	cioslot* result = NULL;
	for(int i = priv->slot_idx; i < priv->buff->slot_count; i++){
		if(!priv->buff->slots[i].__in_use){
			priv->slot_idx = i;
			result = &priv->buff->slots[i];
		}
	}

	if(!result){
		return CIO_ENOSLOTS;
	}

	//TODO XXX: Add padding logic here!!!
	result->data_len = read(priv->fd,result->slot_start,result->slot_len);
	if(result->data_len == 0){
		return CIO_EEMPTY;
	}

	if(result->data_len < 0){
		if(errno == EAGAIN || errno == EWOULDBLOCK){
			return CIO_ETRYAGAIN;
		}

		return errno;
	}


	return CIO_ENOERROR;
}


/**
 * Try to acquire a slot for writing data into.  You can hang on to this slot as long as you like, but beware, most
 * streams offer a limited number of slots, and some streams offer only one. If you are using stream association for
 * zero-copy data movement, calling read_aquire has the effect as calling write_acquire.
 * Returns:
 *  - ENOERROR: Completed successfully, sloto contains a valid structure.
 *  - ENOSLOTS: The stream could not allocate more slots for the read. Free some slots by releasing a read or write
 *              transaction.
 */
static int strm_write_aquire(ciostrm* this, cioslot* slot_o)
{
	return 0;
}


/* Try to write data described by sloto_i to the given stream called “this”.  If auto_release is set to true,
 * write_commit will release the resources associated with the slot when it is done. Some streams support bulk transfer
 * options to amortise the cost of system calls. For these streams, you can set enqueue to true. Release()
 * with enqueue set to 0 will flush the queue. Write_release() can be called with a NULL slot_i parameter to flush
 * the queue. Unlike POSIX, write_comit() returns the number of bytes remaining rather than the number of bytes sent
 * to make it easy to wait until a write is committed.
 * Returns:
 * - ENOERROR: Completed successfully.
 * - EQFULL: The queue is full. Cannot enqueue more slots. Call write with enqueue set to 0.
 * - ECOPYOP: A copy operation was required to complete this commit.
 * - or: the bytes remaining for this stream. Since writing is non-blocking, this may not always be 0.
 */
static int strm_write_commit(ciostrm* this, cioslot* slot_i, int auto_release,  int enqueue)
{
	return 0;
}


/**
 * Relinquish resources associated with the the slot. Some streams support asynchronous update modes. Release() will
 * check to see if the data in this slot is still valid. If it is not valid, it may have been trashed while you were
 * working on it, so results are invalid! If you are concerned about correctness, you should a) not use a stream
 * that supports asynchronous updates b) ensure that your software can “roll back” c) copy data and check for
 * validity before proceeding.
 * Return values:
 * - ENOERROR: All good, please continue
 * - EINVALID: Your data got trashed, time to recover!
 */
static int strm_release(ciostrm* this, cioslot* slot_i)
{
	return 0;
}


/**
 * Free resources associated with this stream, but not with its connector.
 * Connectors should be free'd separately
 */
static void strm_destroy(ciostrm* this)
{
    if(!this){
        return;
    }

    strm_private* priv = (strm_private*)this->__priv;
    if(priv){
    	free_malloc_buffer(priv->buff);
        free(priv);
    }

    free(this);
}



static int strm_ready(cioselable* this)
{
	return 0;
}


//Make a new fileio stream
static int new_ciostrm_fileio( conn_private* conn_priv, ciostrm** ciostrm_o)
{
    int result = CIO_ENOERROR;

    //Make a new connector
    ciostrm* stream = calloc(1,sizeof(ciostrm));
    if(!stream){
        return CIO_ENOMEM;
    }

    //Populate it
    stream->read_acquire = strm_read_acquire;
    stream->write_aquire = strm_write_aquire;
    stream->write_commit = strm_write_commit;
    stream->release		 = strm_release;
    stream->destroy		 = strm_destroy;

    //Make a new private structure
    stream->__priv = calloc(0,sizeof(strm_private));
    if(!stream->__priv){
        return CIO_ENOMEM;
    }

    //Populate it
    strm_private* priv   = stream->__priv;
    priv->fd			 = conn_priv->fd; //Link back so we can recover from the selectable
    int ret = new_malloc_buffer(8,4096,&priv->buff); //TODO XXX: These are nasty constants, should be fixed
    if(ret){
    	return ret;
    }

    //Set up the selector
    stream->selectable.ready  = strm_ready;
    stream->selectable.fd     = priv->fd;
    stream->selectable.__priv = priv;

    //Set up the stream info
    stream->info->can_read_off 		= true;
    stream->info->has_async_arrv 	= false;
    stream->info->is_bytestream 	= true;
    stream->info->is_encrypted		= false;
    stream->info->is_reliable		= true;
    stream->info->is_thread_safe	= false; //XXX TODO: The answer here should be yes. Butt...
    										 //But it's tricky. Thread safety should be a compile time AND runtime option.
    										 //Thread safety is costly, and we only want it in if we REALY need it.
    stream->info->mtu				= 0; //XXX TODO: Don't know yet. This is buffer size dependent
    stream->info->scope				= 1;

    //Output the stream
    *ciostrm_o = stream;

    //Done!
    return CIO_ENOERROR;
}



static int conn_connect( cioconn* this, ciostrm** ciostrm_o )
{
    conn_private* priv = (conn_private*)this->__priv;
    priv->fd = open(priv->filename,priv->flags | O_NONBLOCK);
    if(priv->fd < 0){
    	return errno;
    }

    return new_ciostrm_fileio(priv,ciostrm_o);
}



static void conn_destroy(cioconn* this)
{
    if(!this){
        return;
    }

    conn_private* priv = (conn_private*)this->__priv;
    if(priv){
        free(priv);
    }

    free(this);
}


static int conn_ready(cioselable* this)
{
	//Connector is ready until it is connected
	conn_private* priv = (conn_private*) this->__priv;
	if(priv->fd < 0){
		return 1;
	}

	return 0;
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
    connector->connect          = conn_connect;
    connector->destroy          = conn_destroy;

    //Make a new private structure
    connector->__priv = calloc(0,sizeof(conn_private));
    if(!connector->__priv){
        return CIO_ENOMEM;
    }

    //Populate it
    conn_private* priv   = connector->__priv;
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
    connector->selectable.ready  = conn_ready;
    connector->selectable.fd     = -1; //Can only use this in a spinner selector right now. TODO XXX: Fake this out.
    connector->selectable.__priv = priv; //Close the loop

    //Output the connector
    *cioconn_o = connector;

    //Done!
    return CIO_ENOERROR;
}
