/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   04 Jul 2015
 *  File name: bring_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */




/*
 * CamIO - The Cambridge Input/Output API
 * Copyright (c) 2015, All rights reserved.
 * See LICENSE.txt for full details.
 *
 *  Created:   04 Jul 2015
 *  File name: bring_device.h
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
#include <stddef.h>

#include <src/camio.h>
#include <src/types/device_params_vec.h>
#include <src/devices/device.h>
#include <src/camio.h>

#include <deps/chaste/utils/util.h>
#include <deps/chaste/data_structs/circular_queue/circular_queue.h>
#include <deps/chaste/utils/debug.h>

#include "bring_channel.h"
#include "bring_device.h"


static const char* const scheme = "bring";

#define CAMIO_BRING_SLOT_COUNT_DEFAULT 1024
#define CAMIO_BRING_SLOT_SIZE_DEFAULT (1024-sizeof(bring_slot_header_t))

#define getpagesize() sysconf(_SC_PAGESIZE)


/**************************************************************************************************************************
 * PER DEVICE STATE
 **************************************************************************************************************************/
typedef struct bring_priv_s {

    bring_params_t* params;     //Parameters used when a connection happens

    bool is_devected;          //Has devect be called?
    int bring_fd;               //File descriptor for the backing buffer

    ch_cbq_t* chan_req_queue; //Queue for channel requests

    //Actual data structure
    volatile bring_header_t* bring_head;

    char* bring_filen;

} bring_device_priv_t;



/**************************************************************************************************************************
 * Connect functions
 **************************************************************************************************************************/

camio_error_t bring_channel_request( camio_device_t* this, camio_msg_t* req_vec, ch_word* vec_len_io)
{
    //DBG("Doing bring channel request...!\n");

    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    if(unlikely(NULL == cbq_push_back_carray(priv->chan_req_queue, req_vec,vec_len_io))){
        ERR("Could not push any items on to queue.");
        return CAMIO_EINVALID;
    }

    //DBG("Bring channel request done - %lli requests added\n", *vec_len_io);
    return CAMIO_ENOERROR;
}


static camio_error_t bring_devect_peek_server(camio_device_t* this)
{

    camio_error_t result = CAMIO_ENOERROR;
    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    //DBG("Doing devect peek server on %s\n", priv->bring_filen);

    if(priv->bring_fd > -1 ){ //Ready to go! Call devect!
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

static camio_error_t bring_devect_peek_client(camio_device_t* this)
{

    camio_error_t result = CAMIO_ENOERROR;
    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
    //DBG("Doing devect peek client on %s\n", priv->bring_filen);

    if(priv->bring_fd > -1 ){ //Ready to go! Call devect!
        return CAMIO_ENOERROR;
    }

    //Now there is a bring file and it should have a header in it
    int bring_fd = open(priv->bring_filen, O_RDWR);
    if(bring_fd < 0){
        //ERR("Could not open named shared memory file \"%s\". Error=%s\n", priv->bring_filen, strerror(errno));
        result = CAMIO_ETRYAGAIN;
        goto error_no_cleanup;
    }

    //This is a sort of nasty atomic access. Which doesn't leave crap lying around like POSIX semaphores do
    bring_header_t header_tmp = {0};
    size_t bytes = read(bring_fd,&header_tmp.magic,sizeof(ch_word));
    if(bytes < sizeof(ch_word)){
        //ERR("Could not read header magic. Not enough bytes, expected %lli but got %lli\n", sizeof(header_tmp.magic), bytes);
        result = CAMIO_ETRYAGAIN;
        goto error_close_file;
    }
    if(header_tmp.magic != BRING_MAGIC_SERVER){
        //ERR("Bad magic! Expected 0x%016X but got 0x%016X\n", BRING_MAGIC_SERVER, header_tmp.magic);
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

    //Remove the filename from the filesystem. Since the and reader are both still devected
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



//Try to see if devecting is possible.
static camio_error_t bring_devect_peek(camio_device_t* this)
{
    //DBG("Doing devect peek\n");
    camio_error_t result = CAMIO_ENOERROR;
    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    if(priv->params->server){
        result = bring_devect_peek_server(this);
        if(result != CAMIO_ENOERROR){
            return result;
        }

        __sync_synchronize();
        if(priv->bring_head->magic == (volatile ch_word)BRING_MAGIC_CLIENT){
            //DBG("Head magic found!\n");
            return CAMIO_ENOERROR;
        }

        //DBG("Returning %lli\n", CAMIO_ETRYAGAIN);
        return CAMIO_ETRYAGAIN;

    }
    else{
        result = bring_devect_peek_client(this);
    }

    return result;

}


static camio_error_t bring_channel_ready(camio_muxable_t* this)
{
    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this->parent.device);
    //DBG("Doing channel ready\n");

    //DBG("req_queue count=%lli\n", priv->chan_req_queue->count);
    if(priv->chan_req_queue->count == 0){
        //DBG("No channel requests yet received\n");
        return CAMIO_ETRYAGAIN;
    }

    return bring_devect_peek(this->parent.device);
}

static camio_error_t bring_channel_result(camio_device_t* this, camio_msg_t* res_vec, ch_word* vec_len_io )
{
    DBG("Getting bring devect result\n");
    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    //Is there any data waiting? If not, try to get some
    if(unlikely(priv->chan_req_queue->count <= 0)){
        //DBG("No requests have been added, are you sure you called request first?\n");
        camio_error_t err = bring_devect_peek(this);
        if(err){
            //DBG("There are no channels available to return. Did you use chan_ready()?\n");
            *vec_len_io = 0;
            return err;
        }
    }

    const ch_word count = MIN(priv->chan_req_queue->count, *vec_len_io);
    *vec_len_io = count;
    for(ch_word i = 0; i < count; i++, cbq_pop_front(priv->chan_req_queue)){
        camio_msg_t* msg = cbq_peek_front(priv->chan_req_queue);
        if(msg->type == CAMIO_MSG_TYPE_IGNORE){
            continue; //We don't care about this message
        }

        if(msg->type != CAMIO_MSG_TYPE_CHAN_REQ){
            ERR("Expected a channel request message (%lli) but got %lli instead.\n",CAMIO_MSG_TYPE_CHAN_REQ, msg->type);
            continue;
        }


        res_vec[i] = *msg;
        res_vec[i].type = CAMIO_MSG_TYPE_CHAN_RES;
        camio_chan_res_t* res = &res_vec[i].ch_res;

        if(priv->is_devected){
            DBG("Only channel already supplied! No more channels available\n");
            res->status = CAMIO_EALLREADYCONNECTED; // We're already devected!
            //Error code is in the request, the result is returned successfully even though the result was not a success
            continue;
        }

        res->channel = NEW_CHANNEL(bring);
        if(!res->channel){
            res->status = CAMIO_ENOMEM;
            ERR("No memory trying to allocate channel!\n");
            //Error code is in the request, the result is returned successfully even though the result was not a success
            continue;
        }

        camio_error_t err = bring_channel_construct(res->channel, this, priv->bring_head, priv->params, priv->bring_fd);

        if(err){
            res->status = err;
            continue;
        }

        priv->is_devected = true;
        res->status = CAMIO_ENOERROR;
        DBG("Done successfully returning channel result\n");
    }

    DBG("Returning %lli channel results\n", count);
    return CAMIO_ENOERROR;

}



/**************************************************************************************************************************
 * Setup and teardown
 **************************************************************************************************************************/

static camio_error_t bring_construct(camio_device_t* this, void** params, ch_word params_size)
{

    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this);
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

    priv->chan_req_queue = ch_cbq_new(1,sizeof(void*)); //Only 1 request slot, since we can (currently) only devect once

    priv->bring_fd = -1;

    priv->bring_filen = (char*)calloc(1,priv->params->hierarchical.str_len + 1);
    strncpy(priv->bring_filen,priv->params->hierarchical.str, priv->params->hierarchical.str_len);

    //Populate the rest of the muxable structure

    return CAMIO_ENOERROR;
}


static void bring_destroy(camio_device_t* this)
{
    DBG("Destroying bring device\n");
    bring_device_priv_t* priv = DEVICE_GET_PRIVATE(this);

    if(priv->bring_filen)      { free(priv->bring_filen);       }

    if(priv->params) { free(priv->params); }
    DBG("Freed params\n");
    free(this);
    DBG("Freed device structure\n");
}


NEW_DEVICE_DEFINE(bring, bring_device_priv_t)



static camio_error_t new(void** params, ch_word params_size, camio_device_t** device_o)
{
    camio_device_t* dev = NEW_DEVICE(bring);
    if(!dev){
        *device_o = NULL;
        return CAMIO_ENOMEM;
    }

    *device_o = dev;

    return dev->vtable.construct(dev,params, params_size);
}


void bring_init()
{

    DBG("Initializing bring...\n");
    CH_VECTOR(CAMIO_TRANSPORT_PARAMS_VEC)* params = CH_VECTOR_NEW(CAMIO_TRANSPORT_PARAMS_VEC,256,
            CH_VECTOR_CMP(CAMIO_TRANSPORT_PARAMS_VEC));
    if(!params){
        return; // No memory. Can't register this device
    }

    add_param_optional(params,"slots",bring_params_t,slots, CAMIO_BRING_SLOT_COUNT_DEFAULT);
    add_param_optional(params,"slot_sz",bring_params_t,slot_sz,CAMIO_BRING_SLOT_SIZE_DEFAULT);
    add_param_optional(params,"server",bring_params_t,server, 0);
    add_param_optional(params,"expand",bring_params_t,expand, 1);
    const ch_word hier_offset = offsetof(bring_params_t,hierarchical);

    register_new_device(scheme,strlen(scheme),hier_offset,new,sizeof(bring_params_t),params,0);
    DBG("Initializing bring...Done\n");
}
