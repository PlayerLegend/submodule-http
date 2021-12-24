#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#define FLAT_INCLUDES
#include "../array/range.h"
#include "../window/def.h"
#include "../window/alloc.h"
#include "../keyargs/keyargs.h"
#include "../convert/def.h"
#include "../convert/fd.h"
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

    convert_interface_fd fd_out = convert_interface_fd_init(STDOUT_FILENO);

    convert_interface * get;

    window_unsigned_char window = {0};

    window_alloc (window, 1e6);
    
    bool error = false;
    
    for (int i = 3; i < argc; i++)
    {
	get = http_client_get(client, argv[i]);
	
	while (convert_fill(&error, &window, get))
	{
	    if (!convert_drain (&error, &window, &fd_out.interface))
	    {
		log_fatal ("A write error occurred");
	    }
	}

	if (error)
	{
	    log_fatal ("A read error occurred");
	}

	http_get_free (get);
    }
    
    http_client_disconnect(client);

    //chain_write_free(fd_out);
    
    return 0;
    
fail:
    return 1;
}
