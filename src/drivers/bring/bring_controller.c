/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_controller.h
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


#include <src/devices/controller.h>
#include <src/camio.h>
#include <src/camio_debug.h>

#include <deps/chaste/utils/util.h>
#include <deps/chaste/data_structs/circular_buffer/circular_buffer.h>

#include "bring_device.h"
#include "bring_controller.h"
#include "bring_channel.h"

#define getpagesize() sysconf(_SC_PAGESIZE)

/**************************************************************************************************************************
 * PER CONTROLLER STATE
 **************************************************************************************************************************/
typedef struct bring_priv_s {

    bring_params_t* params;     //Parameters used when a connection happens

    bool is_connected;          //Has connect be called?
    int bring_fd;               //File descriptor for the backing buffer

    ch_cbuff_t* chan_req_queue; //Queue for channel requests

    //Actual data structure
    volatile bring_header_t* bring_head;

    char* bring_filen;

} bring_controller_priv_t;



/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

camio_error_t bring_channel_request( camio_controller_t* this, camio_chan_req_t* req_vec, ch_word vec_len)
{
    DBG("Doing bring channel request...!\n");

    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this);

    if(priv->is_connected){
        DBG("Only channel already supplied! Why are you calling this twice?\n");
        return CAMIO_ETOOMANY; // We're already connected!
    }

    int err = 0;
    DBG("Pushing %lli items into channel request queue of size %lli with %lli items in it\n",
            vec_len,
            priv->chan_req_queue->size,
            priv->chan_req_queue->count
    );
    if(unlikely( (err = cbuff_push_back_carray(priv->chan_req_queue, req_vec,vec_len)) )){
        ERR("Could not push %lli items on to queue. Error =%lli\n", vec_len, err);
        if(err == CH_CBUFF_TOOMANY){
            ERR("Request queue is full\n");
            return CAMIO_ETOOMANY;
        }
        return CAMIO_EINVALID;
    }
    DBG("req_queue count=%lli\n", priv->chan_req_queue->count);

    DBG("Bring read request done\n");
    return CAMIO_ENOERROR;
}


static camio_error_t bring_connect_peek_server(camio_controller_t* this)
{

    camio_error_t result = CAMIO_ENOERROR;
    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this);
    DBG("Doing connect peek server on %s\n", priv->bring_filen);

    if(priv->bring_fd > -1 ){ //Ready to go! Call connect!
        return CAMIO_ENOERROR;
    }

    DBG("Making bring called %.*s with %lu slots of size %lu\n",
        priv->params->hierarchical.str_len,
        priv->params->hierarchical.str,
        priv->params->slots,
        priv->params->slot_sz
    );

    //See if a bring file already exists, if so, get rid of it.
    int bring_fd = open(priv->bring_filen, O_RDONLY);
    if(bring_fd > 0){
        DBG("Found stale bring file. Trying to remove it.\n");
        close(bring_fd);
        if( unlink(priv->bring_filen) < 0){
            ERR("Could not remove stale bring file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
            result = CAMIO_EINVALID;
            goto error_no_cleanup;
        }
    }

    bring_fd = open(priv->bring_filen, O_RDWR | O_CREAT | O_TRUNC , (mode_t)(0666));
    if(bring_fd < 0){
        ERR("Could not open file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
        result = CAMIO_EINVALID;
        goto error_no_cleanup;

    }

    //Calculate the amount of memory we will need
    //Each slot has a requested size, plus some header
    const ch_word mem_per_slot      = priv->params->slot_sz + sizeof(bring_slot_header_t);
    //Round up each slot so that it's a multiple of 64bits.
    const ch_word slot_aligned_size = round_up(mem_per_slot, sizeof(int64_t));
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
                priv->bring_filen,
            bring_total_mem,
            strerror(errno)
        );
        result = CAMIO_EINVALID;
        goto close_file_error;
    }

    //Stick some data in the file to make the sizing stick
    if(write(bring_fd, "", 1) != 1){
        ERR( "Could not write file in shared region \"%s\" at size=%lli. Error=%s\n",
                priv->bring_filen,
            bring_total_mem,
            strerror(errno)
        );
        result = CAMIO_EINVALID;
        goto close_file_error;
    }

    //Map the file into memory
    void* mem = mmap( NULL, bring_total_mem, PROT_READ | PROT_WRITE, MAP_SHARED, bring_fd, 0);
    if(unlikely(mem == MAP_FAILED)){
        ERR("Could not memory map bring file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
        result = CAMIO_EINVALID;
        goto  close_file_error;
    }
    DBG("memory mapped at addres =%p\n", mem);
    //Memory must be page aligned otherwise we're in trouble (TODO - could pass alignment though the file and check..)
    if( ((uint64_t)mem) != (((uint64_t)mem) & ~0xFFF)){
        ERR("Could not memory map bring file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
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
    bring_head->rd_slots              = MIN(bring_head->rd_mem_len / bring_head->rd_slots_size, priv->params->slots);
    bring_head->wr_mem_start_offset   = round_up(bring_head->rd_mem_start_offset + bring_head->rd_mem_len, getpagesize());
    bring_head->wr_mem_len            = priv->params->expand ? round_up(mem_per_ring,getpagesize()) : mem_per_ring;
    bring_head->wr_slots_size         = slot_aligned_size;
    bring_head->wr_slots              = MIN(bring_head->wr_mem_len / bring_head->wr_slots_size, priv->params->slots);
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
        priv->bring_filen,
        priv->bring_head->rd_slots,
        priv->bring_head->rd_slots_size,
        priv->bring_head->rd_slots_size - sizeof(bring_slot_header_t)
    );

    priv->bring_fd = bring_fd;

    //Finally, tell the client that we're ready to party
    //1 - Make sure all the memory writes are done
    __sync_synchronize();
    //2 - Do a word aligned single word write (atomic)
    priv->bring_head->magic = BRING_MAGIC_SERVER;
    //3 - Again, make sure all the memory writes are done
    __sync_synchronize();

    result = CAMIO_ENOERROR;
    return result;

close_file_error:
    close(bring_fd);
    return result;

error_no_cleanup:
    return result;

}

static camio_error_t bring_connect_peek_client(camio_controller_t* this)
{

    camio_error_t result = CAMIO_ENOERROR;
    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this);
    DBG("Doing connect peek client on %s\n", priv->bring_filen);

    if(priv->bring_fd > -1 ){ //Ready to go! Call connect!
        return CAMIO_ENOERROR;
    }

    //Now there is a bring file and it should have a header in it
    int bring_fd = open(priv->bring_filen, O_RDWR);
    if(bring_fd < 0){
        ERR("Could not open named shared memory file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
        result = CAMIO_ETRYAGAIN;
        goto error_no_cleanup;
    }

    //This is a sort of nasty atomic access. Which doesn't leave crap lying around like POSIX semaphores do
    bring_header_t header_tmp = {0};
    size_t bytes = read(bring_fd,&header_tmp.magic,sizeof(ch_word));
    if(bytes < sizeof(ch_word)){
        ERR("Could not read header magic. Not enough bytes, expected %lli but got %lli\n", sizeof(header_tmp.magic), bytes);
        result = CAMIO_ETRYAGAIN;
        goto error_close_file;
    }
    if(header_tmp.magic != BRING_MAGIC_SERVER){
        ERR("Bad magic! Expected 0x%016X but got 0x%016X\n", BRING_MAGIC_SERVER, header_tmp.magic);
        result = CAMIO_ETRYAGAIN;
        goto error_close_file;
    }
    header_tmp.magic = BRING_MAGIC_CLIENT;

    //If we have the magic value, try to read the rest
    const size_t size_no_magic =  sizeof(header_tmp) - sizeof(header_tmp.magic);
    bytes = read(bring_fd,&header_tmp.total_mem, size_no_magic);
    if(bytes < size_no_magic){
        ERR("Could not read header. Not enough bytes, expected %lli but got %lli\n", size_no_magic, bytes);
        result = CAMIO_ETRYAGAIN;
        goto error_close_file;
    }

    bring_header_t* bring_head = &header_tmp;
    (void)bring_head;
    DBG("bring memory offsets (from file)\n");
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
        ERR("Could not memory map bring file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
        result = CAMIO_EINVALID;
        goto  error_close_file;
    }

    //Memory must be page aligned otherwise we're in trouble (TODO - could pass alignment though the file and check..)
    DBG("%lli bytes of memory mapped at address =%p\n", header_tmp.total_mem, mem);
    if( ((uint64_t)mem) != (((uint64_t)mem) & ~0xFFF)){
        ERR("Could not memory map bring file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
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
    if(unlink(priv->bring_filen) < 0){
        ERR("Could not remove bring file \"%s\". Error = \"%s\"", priv->bring_filen, strerror(errno));
        result = CAMIO_EINVALID;
        goto error_unlock_mem;
    }

    priv->bring_fd = bring_fd;
    priv->bring_head = mem;

    bring_head = mem;
    (void)bring_head;
    DBG("bring memory offsets (from mmap)\n");
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

    __sync_synchronize();
    priv->bring_head->magic = (volatile ch_word)BRING_MAGIC_CLIENT;
    __sync_synchronize();

    return CAMIO_ENOERROR;

error_unlock_mem:
    munlock(mem, header_tmp.total_mem);

error_unmap_file:
    munmap(mem, header_tmp.total_mem);

error_close_file:
    close(bring_fd);

error_no_cleanup:
    return result;
}



//Try to see if connecting is possible.
static camio_error_t bring_connect_peek(camio_controller_t* this)
{
    DBG("Doing connect peek\n");
    camio_error_t result = CAMIO_ENOERROR;
    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this);

    if(priv->params->server){
        result = bring_connect_peek_server(this);
        if(!result == CAMIO_ENOERROR){
            return result;
        }

        __sync_synchronize();
        if(priv->bring_head->magic == (volatile ch_word)BRING_MAGIC_CLIENT){
            DBG("Head magic found!\n");
            return CAMIO_ENOERROR;
        }

        return CAMIO_ETRYAGAIN;

    }
    else{
        result = bring_connect_peek_client(this);
    }

    return result;

}


static camio_error_t bring_channel_ready(camio_muxable_t* this)
{

    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this->parent.controller);
    DBG("Doing channel ready\n");

    DBG("req_queue count=%lli\n", priv->chan_req_queue->count);
    if(priv->chan_req_queue->count == 0){
        DBG("No channel requests yet received\n");
        return CAMIO_ETRYAGAIN;
    }

    camio_error_t err = bring_connect_peek(this->parent.controller);
    if(err){
        return err;
    }

    camio_chan_req_t** req_p = NULL;
    req_p = cbuff_use_front(priv->chan_req_queue);
    if(!req_p){
        ERR("WTF? There should be a request here for us to use!\n");
        return CAMIO_EINVALID;
    }

    return err;
}

static camio_error_t bring_channel_result(camio_controller_t* this, camio_chan_req_t** res_o )
{
    DBG("Doing bring connect result\n");
    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->chan_req_queue->_used_index <= 0)){
        camio_error_t err = bring_connect_peek(this);
        if(err){
            DBG("There are no channels available to return. Did you use chan_ready()?\n");
            return err;
        }
    }

    camio_chan_req_t** req_p = cbuff_peek_front(priv->chan_req_queue);
    camio_chan_req_t* res = *req_p;
    res->channel = NEW_CHANNEL(bring);
    if(!res->channel){
        res->status = CAMIO_ENOMEM;
        //Error code is in the request, the result is returned successfully even though the result was not a success
        return CAMIO_ENOERROR;
    }


    camio_error_t err = bring_channel_construct(res->channel, this, priv->bring_head, priv->params, priv->bring_fd);
    if(err){
       return err;
    }

    cbuff_pop_front(priv->chan_req_queue);
    *res_o = *req_p;

    priv->is_connected = true;
    return CAMIO_ENOERROR;
}



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t bring_construct(camio_controller_t* this, void** params, ch_word params_size)
{

    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this);
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

    priv->chan_req_queue = ch_cbuff_new(1,sizeof(void*)); //Only 1 request slot, since we can (currently) only connect once

    priv->bring_fd = -1;

    priv->bring_filen = (char*)calloc(1,priv->params->hierarchical.str_len + 1);
    strncpy(priv->bring_filen,priv->params->hierarchical.str, priv->params->hierarchical.str_len);

    //Populate the rest of the muxable structure

    return CAMIO_ENOERROR;
}


static void bring_destroy(camio_controller_t* this)
{
    DBG("Destroying bring controller\n");
    bring_controller_priv_t* priv = CONTROLLER_GET_PRIVATE(this);

    if(priv->bring_filen)      { free(priv->bring_filen);       }

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed controller structure\n");
}


NEW_CONTROLLER_DEFINE(bring, bring_controller_priv_t)
