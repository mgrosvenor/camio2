/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: netmap_device.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */


#include "netmap_device.h"
#include "netmap_channel.h"
#include "../../camio.h"
#include "../../camio_debug.h"
#include "netmap.h"
#include "netmap_user.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <memory.h>




typedef struct netmap_priv_s {
    ch_bool sw_ring;
    ch_bool hw_ring;
    ch_word ring_id;
    ch_cstr path;
    ch_cstr dev;
} netmap_priv_t;




static camio_error_t netmap_construct(camio_dev_t* this, ch_cstr dev, ch_cstr path, ch_bool sw_ring, ch_bool hw_ring,
        ch_word ring_id)
{

    netmap_priv_t* priv = DEVICE_GET_PRIVATE(this);

    //Not much to do here. The device doesn't have much internal state that it needs.

    //TODO Should think about making a copy here since these strings could go away between now and when we want to use them
    priv->dev       = dev;
    priv->path      = path;
    priv->sw_ring   = sw_ring;
    priv->hw_ring   = hw_ring;
    priv->ring_id   = ring_id;

    return CAMIO_ENOERROR;

}




static void netmap_destroy(camio_dev_t* this)
{
    netmap_priv_t* priv = DEVICE_GET_PRIVATE(this);
    (void)priv;
}




static camio_error_t netmap_construct_str(camio_dev_t* this, camio_uri_t* uri)
{
    netmap_priv_t* priv = DEVICE_GET_PRIVATE(this);
    (void)priv;
    (void)uri;

    // TODO XXX: Convert the URI into a bunch of opts here...

    ch_cstr dev       = "eth0";
    ch_cstr path      = "/dev/netmap";
    ch_bool sw_ring   = false;
    ch_bool hw_ring   = true;
    ch_word ring_id   = 0;

    return netmap_construct(this,dev,path,sw_ring,hw_ring, ring_id);
}




static camio_error_t netmap_construct_bin(camio_dev_t* this, va_list args)
{
    netmap_priv_t* priv = DEVICE_GET_PRIVATE(this);
    (void)priv;
    (void)args;

    // TODO XXX: Convert the va_list into a bunch of opts here

    ch_cstr dev             = "eth0";
    ch_cstr path            = "/dev/netmap";
    ch_bool sw_ring   = false;
    ch_bool hw_ring   = true;
    ch_word ring_id   = 0;

    return netmap_construct(this,dev,path,sw_ring,hw_ring, ring_id);

}




static camio_error_t netmap_connect(camio_dev_t* this, camio_channel_t** channel_o )
{
    netmap_priv_t* priv = DEVICE_GET_PRIVATE(this);

    int netmap_fd = -1;
    struct nmreq req;

    //Some helpful aliases to make the code clearner later
    //const ch_cstr path      = priv->path;
    //const ch_cstr dev       = priv->dev;
    const ch_bool sw_ring   = priv->sw_ring;
    const ch_bool hw_ring   = priv->hw_ring;
    const ch_word ring_id   = priv->ring_id;


    DBG("Opening %s\n", path);
    netmap_fd = open(priv->path, O_RDWR);
    if(unlikely(netmap_fd < 0)){
        DBG( "Could not open file \"%s\". Error=%s\n", "/dev/netmap", strerror(errno));
        return CAMIO_EINVALID;//TODO XXX FIXME
    }

    //Request a specific interface
    memset(&req, 0, sizeof(req));
    req.nr_version = NETMAP_API;
    strncpy(req.nr_name, priv->dev, sizeof(req.nr_name));

    if(ioctl(netmap_fd, NIOCGINFO, &req)){
        DBG( "Could not register netmap interface %s\n", dev);
        return CAMIO_EINVALID;//TODO XXX FIXME use the correct error ID
    }

    //These hold the range of valid rings to look at
    ch_word rings_start = -1;
    ch_word rings_end   = -1;


    // From netmap.h
    // all the NIC rings        0x0000  -
    // only HOST ring           0x2000  -
    // single NIC ring          0x4000 + ring index
    // all the NIC+HOST rings   0x6000  -
    if(sw_ring && !hw_ring){ //SW ring only
        req.nr_ringid = 0x2000;

        rings_start = req.nr_rx_rings + 1; //Software ring is located at ring n + 1
        rings_end   = rings_start + 1;
    }
    else if(!sw_ring && hw_ring && ring_id < 0){//Use all HW rings
        req.nr_ringid = 0x0000;

        rings_start = 0;
        rings_end   = req.nr_rx_rings;

    }
    else if(!sw_ring && hw_ring && ring_id >= 0){//a single HW ring
        if(ring_id >= req.nr_rx_rings ){ //Assume the same number of RX and TX rings
            DBG( "Ring ID (%i) is out of range [%i,%i]\n", ring_id, 0,req.nr_rx_rings);
            return CAMIO_EINVALID;//TODO XXX FIXME use the correct error ID
        }
        req.nr_ringid = 0x4000 + ring_id;

        rings_start = ring_id;
        rings_end   = rings_start + 1;

    }
    else if(sw_ring && hw_ring && ring_id < 0){ //Use all HW + SW rings
        req.nr_ringid = 0x6000;

        rings_start = 0;
        rings_end   = req.nr_rx_rings + 1 + 1; //Software ring is located at ring n + 1

    }
    else{
        DBG( "Invaluid request sw_ring=%i hw_ring=%i ring_id=%i\n", sw_ring, hw_ring, ring_id);
        return CAMIO_EINVALID;//TODO XXX FIXME use the correct error ID
    }


    if(ioctl(netmap_fd, NIOCREGIF, &req)){
        DBG( "Could not register netmap interface %s Error=%s\n", dev, strerror(errno));
        return CAMIO_EINVALID;//TODO XXX FIXME use the correct error ID
    }


    DBG("\nVersion    = %u\n"
            "Memsize    = %u\n"
            "Ringid     = 0x%04x\n"
            "Nr Offset  = %u\n"
            "rx rings   = %u\n"
            "rx slots   = %u\n"
            "tx rings   = %u\n"
            "tx slots   = %u\n",
            req.nr_version,
            req.nr_memsize / 1024 / 1204,
            req.nr_ringid,
            req.nr_offset,
            req.nr_rx_rings,
            req.nr_rx_slots,
            req.nr_tx_rings,
            req.nr_tx_slots
    );


    //Get the netmap interface pointer
    void* netmap_region = mmap(0, req.nr_memsize, PROT_WRITE | PROT_READ, MAP_SHARED, netmap_fd, 0);
    if(unlikely(netmap_region == MAP_FAILED)){
        DBG( "Could not memory map netmap region for interface \"%s\". Error=%s\n", dev, strerror(errno));
        return CAMIO_EINVALID;//TODO XXX FIXME use the correct error ID
    }
    struct netmap_if* net_if = NETMAP_IF(netmap_region, req.nr_offset);

    camio_channel_t* channel = NEW_CHANNEL(netmap);
    if(!channel){
        *channel_o = NULL;
        return CAMIO_ENOMEM;
    }
    *channel_o = channel;
    DBG("Stream address=%p (%p)\n",channel, *channel_o);


    return netmap_channel_construct(channel,netmap_fd,net_if,rings_start, rings_end);
}




NEW_DEVICE_DEFINE(netmap, netmap_priv_t)

