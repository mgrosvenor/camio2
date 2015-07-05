/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_connector.h
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <memory.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/sem.h>

#include <src/transports/connector.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include <deps/chaste/utils/util.h>

#include "bring_transport.h"
#include "bring_connector.h"
#include "bring_stream.h"

#define getpagesize() sysconf(_SC_PAGESIZE)

/**************************************************************************************************************************
 * PER STREAM STATE
 **************************************************************************************************************************/
//Header structure -- There is one of these for every
typedef struct bring_header_s {
    ch_word total_mem;              //Total amount of memory needed in the mmapped region

    ch_word rd_mem_start_offset;    //location of the memory region for reads
    ch_word rd_mem_len;             //length of the read memory region
    ch_word rd_slots;               //number of slots in the read region
    ch_word rd_slots_size;          //Size of each slot including the slot footer

    ch_word wr_mem_start_offset;
    ch_word wr_mem_len;
    ch_word wr_slots;
    ch_word wr_slots_size;

} bring_header_t;



typedef struct bring_priv_s {

    bring_params_t* params;     //Parameters used when a connection happens

    bool is_connected;          //Has connect be called?
    int bring_fd;               //File descriptor for the backing buffer

    //Actual data structure
    volatile bring_header_t* bring_head;

    sem_t* mutex;

} bring_connector_priv_t;



/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/


static camio_error_t bring_connect_peek_server(camio_connector_t* this, char* bring_file)
{
    DBG("Doing connect peek server on %s\n", bring_file);
    camio_error_t result = CAMIO_ENOERROR;
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    DBG("Making bring called %.*s with %lu slots of size %lu\n",
        priv->params->hierarchical.str_len,
        priv->params->hierarchical.str,
        priv->params->slots,
        priv->params->slot_sz
    );

    //Make a new named semaphore with the bring file name and initialize it to locked (0)
    priv->mutex = sem_open(bring_file, O_CREAT | O_EXCL, 0666, 0);
    if(priv->mutex == SEM_FAILED){
        ERR("Could not open named semaphore \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto error_no_cleanup;
    }

//--------------------------------------------------------------------------------------------------------------------------
// START ATOMIC SECTION !!!

    //See if a bring file already exists, if so, get rid of it.
    int bring_fd = open(bring_file, O_RDONLY);
    if(bring_fd > 0){
        DBG("Found stale bring file. Trying to remove it.\n");
        close(bring_fd);
        if( unlink(bring_file) < 0){
            ERR("Could not remove stale bring file \"%s\". Error=%s\n", bring_file, strerror(errno));
            result = CAMIO_EINVALID;
            goto error_no_cleanup;
        }
    }

    bring_fd = open(bring_file, O_RDWR | O_CREAT | O_TRUNC , (mode_t)(0666));
    if(bring_fd < 0){
        ERR("Could not open file \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto error_no_cleanup;

    }

    //Calculate the amount of memory we will need
    //Each slot has a requested size, plus some header
    const ch_word mem_per_slot      = priv->params->slot_sz + sizeof(bring_slot_header_t);
    //Round up each slot so that it's a multiple of 64bits.
    const ch_word slot_aligned_size = ((mem_per_slot + sizeof(int64_t) - 1 ) / sizeof(int64_t)) * sizeof(uint64_t);
    //Figure out the total memory commitment for slots
    const ch_word mem_per_ring      = slot_aligned_size * priv->params->slots;
    //Allocate for both server-->client and client-->server connections
    const ch_word total_ring_mem    = mem_per_ring * 2;
    //Include the memory required for the headers -- Make sure there's a place for the synchronization pointer
    const ch_word header_mem        = sizeof(bring_header_t) + sizeof(uint64_t) * 2;
    //All memory required
    const ch_word total_mem_req     = total_ring_mem + header_mem;
    //Add some buffer so that we can align rd/wr rings to page sizes, which will align to a 64 bit boundary as well
    const ch_word aligned_mem       = total_mem_req + getpagesize() * 2;
    const ch_word bring_total_mem   = aligned_mem;

    DBG("bring memory requirements\n");
    DBG("-------------------------\n");
    DBG("mem_per_slot   %lli\n", mem_per_slot);
    DBG("slot_sligned   %lli\n", slot_aligned_size);
    DBG("mem_per_ring   %lli\n", mem_per_ring);
    DBG("total_ring_mem %lli\n", total_ring_mem);
    DBG("header_mem     %lli\n", header_mem);
    DBG("total_mem_req  %lli\n", total_mem_req);
    DBG("aligned_mem    %lli\n", aligned_mem);
    DBG("-------------------------\n");


    //Resize the file
    if(lseek(bring_fd, bring_total_mem -1, SEEK_SET) < 0){
        ERR( "Could not move to end of shared region \"%s\" to size=%lli. Error=%s\n",
            bring_file,
            bring_total_mem,
            strerror(errno)
        );
        result = CAMIO_EINVALID;
        goto close_file_error;
    }

    //Stick some data in the file to make the sizing stick
    if(write(bring_fd, "", 1) != 1){
        ERR( "Could not write file in shared region \"%s\" at size=%lli. Error=%s\n",
            bring_file,
            bring_total_mem,
            strerror(errno)
        );
        result = CAMIO_EINVALID;
        goto close_file_error;

    }

    //Map the file into memory
    void* mem = mmap( NULL, bring_total_mem, PROT_READ | PROT_WRITE, MAP_SHARED, bring_fd, 0);
    if(unlikely(mem == MAP_FAILED)){
        ERR("Could not memory map bring file \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto  close_file_error;
    }
    DBG("memory mapped at addres =%p\n", mem);
    //Memory must be page aligned otherwise we're in trouble (TODO - could pass alignment though the file and check..)
    if( ((uint64_t)mem) != (((uint64_t)mem) & ~0xFFF)){
        ERR("Could not memory map bring file \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto  close_file_error;
    }



    //Pin the pages so that they don't get swapped out
    if(mlock(mem,bring_total_mem)){
        ERR("Could not lock memory map. Error=%s\n", strerror(errno));
        result = CAMIO_EINVALID;
        goto  close_file_error;
    }

    //Populate all the right offsets
    volatile bring_header_t* bring_head = (volatile void*)mem;;
    bring_head->total_mem             = bring_total_mem;
    bring_head->rd_mem_start_offset   = round_up(0 + sizeof(bring_header_t) + sizeof(uint64_t), getpagesize());
    bring_head->rd_mem_len            = priv->params->expand ? round_up(mem_per_ring,getpagesize()) : mem_per_ring;
    bring_head->rd_slots_size         = slot_aligned_size;
    bring_head->rd_slots              = bring_head->rd_mem_len / bring_head->rd_slots_size;
    bring_head->wr_mem_start_offset   = round_up(bring_head->rd_mem_start_offset + bring_head->rd_mem_len, getpagesize());
    bring_head->wr_mem_len            = priv->params->expand ? round_up(mem_per_ring,getpagesize()) : mem_per_ring;
    bring_head->wr_slots_size         = slot_aligned_size;
    bring_head->wr_slots              = bring_head->wr_mem_len / bring_head->wr_slots_size;
    priv->bring_head = bring_head;

    //Here's some debug to figure out if the mappings are correct
    const ch_word rd_end_offset = bring_head->rd_mem_start_offset + bring_head->rd_mem_len -1;
    const ch_word wr_end_offset = bring_head->wr_mem_start_offset + bring_head->wr_mem_len -1;
    (void)rd_end_offset;
    (void)wr_end_offset;
    DBG("bring memory offsets\n");
    DBG("-------------------------\n");
    DBG("total_mem            %016llx (%lli)\n", bring_head->total_mem, bring_head->total_mem);
    DBG("rd_slots             %016llx (%lli)\n", bring_head->rd_slots,  bring_head->rd_slots);
    DBG("rd_slots_size        %016llx (%lli)\n", bring_head->rd_slots_size, bring_head->rd_slots_size);
    DBG("rd_mem_start_offset  %016llx (%lli)\n", bring_head->rd_mem_start_offset, bring_head->rd_mem_start_offset);
    DBG("rd_mem_len           %016llx (%lli)\n", bring_head->rd_mem_len, bring_head->rd_mem_len);
    DBG("rd_mem_end_offset    %016llx (%lli)\n", rd_end_offset, rd_end_offset);

    DBG("wr_slots             %016llx (%lli)\n", bring_head->wr_slots,  bring_head->wr_slots);
    DBG("wr_slots_size        %016llx (%lli)\n", bring_head->wr_slots_size, bring_head->wr_slots_size);
    DBG("wr_mem_start_offset  %016llx (%lli)\n", bring_head->wr_mem_start_offset,bring_head->wr_mem_start_offset);
    DBG("wr_mem_len           %016llx (%lli)\n", bring_head->wr_mem_len, bring_head->wr_mem_len);
    DBG("wr_mem_end_offset    %016llx (%lli)\n", wr_end_offset, wr_end_offset);
    DBG("-------------------------\n");

    DBG("Done making bring called %s with %llu slots of size %llu, usable size %llu\n",
        bring_file,
        priv->bring_head->rd_slots,
        priv->bring_head->rd_slots_size,
        priv->bring_head->rd_slots_size - sizeof(bring_slot_header_t)
    );

    priv->bring_fd = bring_fd;
    result = CAMIO_ENOERROR;

// END ATOMIC SECTION!!!
//---------------------------------------------------------------------------------------------------------------------------

    //Finally, tell the client that we're ready to party
    sem_post(priv->mutex);
    sem_unlink(bring_file);

    return result;

close_file_error:
    close(bring_fd);
    return result;

error_no_cleanup:
    return result;

}

static camio_error_t bring_connect_peek_client(camio_connector_t* this, char* bring_file)
{
    DBG("Doing connect peek client on %s\n", bring_file);
    camio_error_t result = CAMIO_ENOERROR;
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    //Try to open the semaphore
    //Make a new named semaphore with the bring file name and initialize it to locked (0)
    priv->mutex = sem_open(bring_file,0);
    if(priv->mutex == SEM_FAILED){
        if(errno == ENOENT){
            return CAMIO_ETRYAGAIN; // The semaphore has not yet been created
        }

        //EEk some other error happened~
        ERR("Could not open named semaphore \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto error_no_cleanup;
    }

    //Wait until all of the init work is done:
    if(sem_trywait(priv->mutex)){
        if(errno == EAGAIN){
            result = CAMIO_ETRYAGAIN;
            goto error_no_cleanup;
        }

        ERR("Something funny happened on try wait. Error=%s\n", strerror(errno));
        result = CAMIO_EINVALID;
        goto error_no_cleanup;

   }

    //<<-------------- BEGIN ATOMIC SECTION ---------------->>>

    //Now there is a bring file and it should have a header in it
    int bring_fd = open(bring_file, O_RDWR);
    if(bring_fd < 0){
        ERR("Could not open named shared memory file \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto error_release_semaphore;
    }


    bring_header_t header_tmp = {0};
    size_t bytes = read(bring_fd,&header_tmp,sizeof(header_tmp));
    if(bytes < sizeof(bring_header_t)){
        ERR("Could not read header.\n");
        result = CAMIO_EINVALID;
        goto error_close_file;
    }

    bring_header_t* bring_head = &header_tmp;
    (void)bring_head;
    DBG("bring memory offsets\n");
    DBG("-------------------------\n");
    DBG("total_mem            %016llx (%lli)\n", bring_head->total_mem, bring_head->total_mem);
    DBG("rd_slots             %016llx (%lli)\n", bring_head->rd_slots,  bring_head->rd_slots);
    DBG("rd_slots_size        %016llx (%lli)\n", bring_head->rd_slots_size, bring_head->rd_slots_size);
    DBG("rd_mem_start_offset  %016llx (%lli)\n", bring_head->rd_mem_start_offset, bring_head->rd_mem_start_offset);
    DBG("rd_mem_len           %016llx (%lli)\n", bring_head->rd_mem_len, bring_head->rd_mem_len);

    DBG("wr_slots             %016llx (%lli)\n", bring_head->wr_slots,  bring_head->wr_slots);
    DBG("wr_slots_size        %016llx (%lli)\n", bring_head->wr_slots_size, bring_head->wr_slots_size);
    DBG("wr_mem_start_offset  %016llx (%lli)\n", bring_head->wr_mem_start_offset,bring_head->wr_mem_start_offset);
    DBG("wr_mem_len           %016llx (%lli)\n", bring_head->wr_mem_len, bring_head->wr_mem_len);
    DBG("-------------------------\n");


    void* mem = mmap( NULL, header_tmp.total_mem, PROT_READ | PROT_WRITE, MAP_SHARED, bring_fd, 0);
    if(unlikely(mem == MAP_FAILED)){
        ERR("Could not memory map bring file \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto  error_close_file;
    }

    //Memory must be page aligned otherwise we're in trouble (TODO - could pass alignment though the file and check..)
    DBG("memory mapped at addres =%p\n", mem);
    if( ((uint64_t)mem) != (((uint64_t)mem) & ~0xFFF)){
        ERR("Could not memory map bring file \"%s\". Error=%s\n", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto  error_close_file;
    }

    //Pin the pages so that they don't get swapped out
    if(mlock(mem,header_tmp.total_mem)){
        ERR("Could not lock memory map. Error=%s\n", strerror(errno));
        result = CAMIO_EINVALID;
        goto  error_unmap_file;
    }

    //Remove the filename from the filesystem. Since the and reader are both still connected
    //to the file, the space will continue to be available until they both exit.
    if(unlink(bring_file) < 0){
        ERR("Could not remove bring file \"%s\". Error = \"%s\"", bring_file, strerror(errno));
        result = CAMIO_EINVALID;
        goto error_unlock_mem;
    }

    priv->bring_fd = bring_fd;
    priv->bring_head = mem;

error_unlock_mem:
    munlock(mem, header_tmp.total_mem);

error_unmap_file:
    munmap(mem, header_tmp.total_mem);

error_close_file:
    close(bring_fd);

error_release_semaphore:
    sem_post(priv->mutex); //<<-------------- END ATOMIC

error_no_cleanup:
    return result;
}



//Try to see if connecting is possible.
static camio_error_t bring_connect_peek(camio_connector_t* this)
{
    DBG("Doing connect peek\n");
    camio_error_t result = CAMIO_ENOERROR;
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->bring_fd > -1 ){ //Ready to go! Call connect!
        return CAMIO_ENOERROR;
    }

    //Make null terminated file name
    char* bring_file_name = (char*)calloc(priv->params->hierarchical.str_len + 1,1);
    strncpy(bring_file_name,priv->params->hierarchical.str, priv->params->hierarchical.str_len);

    if(priv->params->server){
        result = bring_connect_peek_server(this,bring_file_name);
    }
    else{
        result = bring_connect_peek_client(this,bring_file_name);
    }

    free(bring_file_name);
    return result;

}

static camio_error_t bring_connector_ready(camio_muxable_t* this)
{
    if(this->fd > -1){
        return CAMIO_EREADY;
    }

    camio_error_t err = bring_connect_peek(this->parent.connector);
    if(err == CAMIO_ETRYAGAIN){
        return CAMIO_ENOTREADY;
    }

    if(err != CAMIO_ENOERROR){
        return err;
    }

        return CAMIO_EREADY;
}

static camio_error_t bring_connect(camio_connector_t* this, camio_stream_t** stream_o )
{
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    camio_error_t err = bring_connect_peek(this);
    if(err != CAMIO_ENOERROR){
        return err;
    }

    if(priv->is_connected){
        DBG("Already connected! Why are you calling this twice?\n");
        return CAMIO_EALLREADYCONNECTED; // We're already connected!
    }

    //DBG("Done connecting, now constructing UDP stream...\n");
    camio_stream_t* stream = NEW_STREAM(bring);
    if(!stream){
        *stream_o = NULL;
        return CAMIO_ENOMEM;
    }
    *stream_o = stream;

    err = bring_stream_construct(stream, this);
    if(err){
       return err;
    }

    priv->is_connected = true;
    return CAMIO_ENOERROR;
}



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t bring_construct(camio_connector_t* this, void** params, ch_word params_size)
{

    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    //Basic sanity check that the params is the right one.
    if(params_size != sizeof(bring_params_t)){
        ERR("Bad parameters structure passed\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    bring_params_t* bring_params = (bring_params_t*)(*params);
    priv->params = bring_params;

    //We require a hierarchical part!
    if(bring_params->hierarchical.str_len == 0){
        ERR("Expecting a hierarchical part in the BRING URI, but none was given. e.g my_bring1\n");
        return CAMIO_EINVALID; //TODO XXX : Need better error values
    }
    //But only need a unique name. Not a full file path
    if(memchr(bring_params->hierarchical.str, '/', bring_params->hierarchical.str_len)){
        ERR("Found a / in the bring name.\n");
        return CAMIO_EINVALID;
    }
    if(memchr(bring_params->hierarchical.str, '\\', bring_params->hierarchical.str_len)){
        ERR("Found a \\ in the bring name.\n");
        return CAMIO_EINVALID;
    }
    if(bring_params->rd_only && bring_params->wr_only){
        ERR("Transport cannot be in both read and write mode at the same time!\n");
        return CAMIO_EINVALID;
    }

    priv->bring_fd = -1;

    //Populate the rest of the muxable structure
    this->muxable.mode              = CAMIO_MUX_MODE_CONNECT;
    this->muxable.parent.connector  = this;
    this->muxable.vtable.ready      = bring_connector_ready;
    this->muxable.fd                = -1;

    return CAMIO_ENOERROR;
}


static void bring_destroy(camio_connector_t* this)
{
    DBG("Destroying bring connector\n");
    bring_connector_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed connector structure\n");
}

NEW_CONNECTOR_DEFINE(bring, bring_connector_priv_t)
