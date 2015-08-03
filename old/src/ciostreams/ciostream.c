/*
 * ciochannel.c
 *
 *  Created on: Oct 18, 2013
 *      Author: mgrosvenor
 */
#include "ciochannel.h"
#include "../uri_parser/uri_parser.h"

#include "ciochannel_fileio.h"

typedef struct {
  char* scheme;
  void* global_data;
  int (*strm_new_ciodev)( uri* uri_parsed , struct ciodev_s** ciodev_o, void** global_data );
} cio_channel_entry;


//Should this be in a .h file?? Hmm, feels right, but looks wrong. Like your mum.
static cio_channel_entry cio_channel_registry[] = {
        {"file", NULL, new_ciodev_fileio},
        {0} //Null last entry
};


int new_ciodev( char* uri_str , ciostrm_req* properties, struct ciodev_s** ciodev_o )
{
    uri* uri_parsed;
    int result = parse_uri(uri_str, &uri_parsed);
    if(result){
        return result;
    }

    //For the moment, require all URIs to have a valid scheme and hierarchical part
    //TODO: infer the scheme from the hierarchical part eg -> example.ring where possible
    if( !uri_parsed->scheme_name_len || !uri_parsed->hierarchical_len){
        return CIO_EBADURI;
    }

    //Find the channel that has the same scheme name
    cio_channel_entry* entry = cio_channel_registry;
    for(; entry->scheme; ++entry){
        if(strncmp(entry->scheme, uri_parsed->scheme_name, uri_parsed->scheme_name_len) == 0){
            break;
        }
    }

    //Could not find the channel name type
    if(!entry->scheme){
        return CIO_ENOCHANNEL;
    }

    result = entry->strm_new_ciodev(uri_parsed, ciodev_o, &entry->global_data);


    //TODO XXX: Check properties here
    (void)properties;

    free_uri(&uri_parsed);
    return result;
}
