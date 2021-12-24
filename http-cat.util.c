#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#define FLAT_INCLUDES
#include "../range/def.h"
#include "../window/def.h"
#include "../window/alloc.h"
#include "../keyargs/keyargs.h"
#include "../convert/source.h"
#include "../convert/fd/source.h"
#include "../convert/sink.h"
#include "../convert/duplex.h"
#include "../convert/fd/sink.h"
#include "client.h"
#include "../log/log.h"

int main (int argc, char * argv[])
{
    if (argc < 3)
    {
	log_fatal ("usage: %s [host] [port] [path]", argv[0]);
    }

    const char * host = argv[1];
    const char * port = argv[2];
    
    http_client * client = http_client_connect (host, port);

    window_unsigned_char buffer = {0};

    fd_sink fd_sink = fd_sink_init(.fd = STDOUT_FILENO);

    convert_source * get;

    window_alloc (buffer, 1e6);
    
    for (int i = 3; i < argc; i++)
    {
	get = http_client_get(client, argv[i]);

	get->contents = &buffer;

	if (!convert_join (&fd_sink.sink, get))
	{
	    log_fatal ("Failed to read or write data");
	}
        
	http_get_free (get);
    }
    
    http_client_disconnect(client);

    //chain_write_free(fd_out);
    
    return 0;
    
fail:
    return 1;
}
