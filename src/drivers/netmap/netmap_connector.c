/*
 * CamIO - The Cambridge Input/Output API 
 * Copyright (c) 2014, All rights reserved.
 * See LICENSE.txt for full details. 
 * 
 *  Created:   Aug 15, 2014
 *  File name: netmap_connector.c
 *  Description:
 *  <INSERT DESCRIPTION HERE> 
 */

#include "netmap_transport.h"
#include "netmap_connector.h"
#include "../../camio.h"
#include "../../camio_debug.h"
#include "netmap.h"
#include "netmap_user.h"

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "netmap_if_vec.h"

typedef struct netmap_priv_s {
    int netmap_fd;
    CH_VECTOR(netmap_if)* interfaces;
} netmap_priv_t;


static camio_error_t construct(camio_connector_t* this, ch_cstr dev, ch_cstr path, ch_bool use_host_ring, ch_word ring_id)
{

    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);

    netmap_global_t* global = NULL;
    camio_error_t result = camio_transport_get_global("netm", (void**)&global);
    if(result){
        return result;
    }


    DBG("Netmap  global store at %p\n", global);

    //Already initialised, so we can just reuse that state
    if(global->is_init){
        DBG("Reusing global state\n");
        priv->netmap_fd = global->netmap_fd;
        return CAMIO_ENOERROR;
    }

    DBG("No global state\n");


    int netmap_fd = -1;
    struct nmreq req;

    DBG("Opening %s\n", path);
    netmap_fd = open(path, O_RDWR);
    if(unlikely(netmap_fd < 0)){
        DBG( "Could not open file \"%s\". Error=%s\n", "/dev/netmap", strerror(errno));
        return CAMIO_EINVALID;//TODO XXX FIXME
    }

    //Request a specific interface
    bzero(&req, sizeof(req));
    req.nr_version = NETMAP_API;
    strncpy(req.nr_name, dev, sizeof(req.nr_name));
    req.nr_ringid = 0; //All hw rings

    if(ioctl(netmap_fd, NIOCGINFO, &req)){
        DBG( "Could not get info on netmap interface %s\n",dev);
        return CAMIO_EINVALID;//TODO XXX FIXME
    }

    DBG(  "\nVersion    = %u\n"
            "Memsize    = %u\n"
            "Ringid     = %u\n"
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
            req.nr_tx_slots);


    if(ioctl(netmap_fd, NIOCREGIF, &req)){
        DBG( "Could not register netmap interface %s\n", dev);
        return CAMIO_EINVALID;//TODO XXX FIXME
    }

    DBG("\nMemsize      = %u\n"
            "Ringid     = %u\n"
            "Nr Offset  = %u\n"
            "rx rings   = %u\n"
            "rx slots   = %u\n",
            req.nr_memsize / 1024 / 1204,
            req.nr_ringid,
            req.nr_offset,
            req.nr_rx_rings,
            req.nr_rx_slots);



    global->is_init = true;
    DBG("Global state is initialised\n");

    (void) dev;
    (void) path;
    (void) use_host_ring;
    (void) ring_id;

    return CAMIO_NOTIMPLEMENTED;
}



static camio_error_t construct_str(camio_connector_t* this, camio_uri_t* uri)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)uri;

    // TODO XXX: Convert the URI into a bunch of opts here...

    ch_cstr dev             = "eth0";
    ch_cstr path            = "/dev/netmap";
    ch_bool use_host_ring   = false;
    ch_word ring_id         = 0;

    return construct(this,dev,path,use_host_ring,ring_id);
}

static camio_error_t construct_bin(camio_connector_t* this, va_list args)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)args;

    // TODO XXX: Convert the va_list into a bunch of opts here

    ch_cstr dev             = "eth0";
    ch_cstr path            = "/dev/netmap";
    ch_bool use_host_ring   = false;
    ch_word ring_id         = 0;

    return construct(this,dev,path,use_host_ring,ring_id);

}


static camio_error_t cconnect(camio_connector_t* this, camio_stream_t** stream_o )
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
    (void)stream_o;
    return CAMIO_NOTIMPLEMENTED;
}


static void destroy(camio_connector_t* this)
{
    netmap_priv_t* priv = CONNECTOR_GET_PRIVATE(this);
    (void)priv;
}


CONNECTOR_DEFINE(netmap, netmap_priv_t)

