#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <assert.h>
#define FLAT_INCLUDES
#include "../array/range.h"
#include "../array/string.h"
#include "../window/def.h"
#include "../window/alloc.h"
#include "../convert/def.h"
#include "../convert/fd.h"
#include "../keyargs/keyargs.h"
#include "../log/log.h"
#include "../network/network.h"
#include "../libc/string.h"
#include "../libc/string-extensions.h"
#include "../convert/getline.h"

#define HTTP_VERSION "1.1"

#define CRLF "\r\n"
#define CRLF_LEN 2

typedef struct {
    convert_interface_fd read;
    char * host;
    char * port;
    window_unsigned_char raw;
}
    http_client;

typedef struct {
    convert_interface interface;
    http_client * client;
    enum {
	TRANSFER_ENCODING_IDENTITY,
	TRANSFER_ENCODING_CHUNKED,
    }
	transfer_encoding_type;

    size_t body_size;
    size_t have_size;
}
    convert_interface_http_get;

static bool http_getline (bool * error, range_const_char * line, http_client * client) // should return true if a line is read, false otherwise, and error if input ended before a line could be read
{
    range_const_char end = { .begin = CRLF, .end = end.begin + CRLF_LEN };

    return convert_getline(error, line, &client->raw, &client->read.interface, &end);
}

static void skip_isspace (bool pred, const char ** begin, const char * max)
{
    while (*begin < max)
    {
	if (pred != (bool) isspace (**begin))
	{
	    return;
	}

	(*begin)++;
    }
}

static void split_c (range_const_char * before, range_const_char * after, char c, const range_const_char * input)
{
    size_t sep = range_strchr (input, c);

    before->begin = input->begin;
    before->end = input->begin + sep;
    after->begin = before->end + 1;
    after->end = input->end;
    if (after->begin > after->end)
    {
	after->begin = after->end;
    }
}

static bool parse_http_status (size_t * status, const range_const_char * line)
{
    range_const_char prefix, suffix;

    split_c (&prefix, &suffix, ' ', line);

    if (!range_streq_string (&prefix, "HTTP/" HTTP_VERSION))
    {
	return false;
    }

    if (0 == range_atozd (status, &suffix))
    {
	return false;
    }

    return true;
}

static bool parse_http_line (convert_interface_http_get * get, const range_const_char * line)
{
    range_const_char prefix, suffix;

    split_c (&prefix, &suffix, ':', line);

    skip_isspace (true, &suffix.begin, suffix.end);

    if (range_streq_string (&prefix, "Content-Length"))
    {
	if (get->body_size)
	{
	    log_fatal ("Repeated Content-Length in http header");
	}

	if (get->transfer_encoding_type != TRANSFER_ENCODING_IDENTITY)
	{
	    log_fatal ("Content-Length applied to non-identity transfer encoding");
	}
	
	if (0 == range_atozd (&get->body_size, &suffix))
	{
	    log_fatal ("Non-numerical argument to Content-Length: %.*s", range_count(suffix), suffix.begin);
	}
    }
    else if (range_streq_string (&prefix, "Transfer-Encoding"))
    {
	if (range_streq_string(&suffix, "chunked"))
	{
	    if (get->body_size)
	    {
		log_fatal ("Content-Length set for chunked encoding");
	    }
	    
	    if (get->transfer_encoding_type != TRANSFER_ENCODING_IDENTITY)
	    {
		log_fatal ("Repeated Transfer-Encoding");
	    }

	    get->transfer_encoding_type = TRANSFER_ENCODING_CHUNKED;
	}
	else if (range_streq_string(&suffix, "identity") && get->transfer_encoding_type != TRANSFER_ENCODING_IDENTITY)
	{
	    log_fatal ("Repeated Transfer-Encoding");
	}
	else
	{
	    log_fatal ("Unsupported Transfer-Encoding: %.*s", range_count(suffix), suffix.begin);
	}
    }
    
    return true;
    
fail:
    return false;
}

static bool http_get_contents_chunked_header_func (bool * error, window_unsigned_char * output, convert_interface * interface, size_t limit);
static bool http_get_body_func (bool * error, window_unsigned_char * output, convert_interface * interface, size_t limit)
{
    convert_interface_http_get * get = (convert_interface_http_get*) interface;
    
    if (get->have_size == get->body_size)
    {
	return false;
    }
    
    if (get->body_size == 0)
    {
	window_release (get->client->raw, CRLF_LEN);
	return false;
    }

    size_t max_pull_size = 1e6;
    assert (get->body_size >= get->have_size);
    size_t want_size = get->body_size - get->have_size;
    size_t pull_size = want_size < max_pull_size ? want_size : max_pull_size;

    if (!convert_fill_limit(error, &get->client->raw, &get->client->read.interface, pull_size))
    {
	*error = true;
	return false;
    }

    assert (!*error);
    
    size_t got_size = range_count (get->client->raw.region);

    if (got_size > want_size)
    {
	got_size = want_size;
    }

    get->have_size += got_size;

    window_append_bytes (output, get->client->raw.region.begin, got_size);

    window_release (get->client->raw, got_size);

    assert (get->have_size <= get->body_size);
    
    if (get->have_size == get->body_size)
    {
	if (get->transfer_encoding_type == TRANSFER_ENCODING_CHUNKED)
	{
	    window_release (get->client->raw, CRLF_LEN);
	    interface->read = http_get_contents_chunked_header_func;
	    return true;
	}
	else
	{
	    assert (get->transfer_encoding_type == TRANSFER_ENCODING_IDENTITY);
	    assert (!*error);
    	    return false;
	}
    }
    else
    {
	return true;
    }
}

static bool to_hex (size_t * n, const range_const_char * line)
{
    const char * i;

    *n = 0;

    for_range (i, *line)
    {
	if (*i >= '0' && *i <= '9')
	{
	    *n = (*i - '0') + (*n) * 16;
	}
	else if ( (*i >= 'a' && *i <= 'f') || (*i >= 'A' && *i <= 'F') )
	{
	    *n = (tolower(*i) - 'a') + 10 + (*n) * 16;
	}
	else
	{
	    break;
	}
    }

    return range_index (i, *line) != 0;
}

static bool http_get_contents_chunked_header_func (bool * error, window_unsigned_char * output, convert_interface * interface, size_t limit)
{
    convert_interface_http_get * get = (convert_interface_http_get*) interface;
    
    range_const_char line;

    if (!http_getline (error, &line, get->client))
    {
	return !*error;
    }

    assert (!*error);

    if (!to_hex (&get->body_size, &line))
    {
	log_fatal ("Couldn't parse hex number");
    }
    
    get->have_size = 0;

    interface->read = http_get_body_func;

    return true;
    
fail:
    *error = true;
    return false;
}

static bool http_get_header_func (bool * error, window_unsigned_char * output, convert_interface * interface, size_t limit)
{
    convert_interface_http_get * get = (convert_interface_http_get*) interface;

    range_const_char line;

    if (!http_getline (error, &line, get->client))
    {
	return !*error;
    }
    
    assert (!*error);
    
    if (range_is_empty (line))
    {
	switch (get->transfer_encoding_type)
	{
	case TRANSFER_ENCODING_IDENTITY:
	    interface->read = http_get_body_func;
	    break;
	    
	case TRANSFER_ENCODING_CHUNKED:
	    interface->read = http_get_contents_chunked_header_func;
	    break;
	    
	default:
	    log_fatal ("Invalid transfer encoding type %zu", get->transfer_encoding_type);
	    break;
	}
    }
    else if (!parse_http_line (get, &line))
    {
	log_fatal ("Failed to parse header line: %.*s", (int)range_count(line), line.begin);
    }

    return true;

fail:
    *error = true;
    return false;
}

static bool http_get_status_func (bool * error, window_unsigned_char * output, convert_interface * interface, size_t limit)
{
    convert_interface_http_get * get = (convert_interface_http_get*) interface;
    
    range_const_char line;

    if (!http_getline (error, &line, get->client))
    {
	return !*error;
    }
    
    assert (!*error);
    
    size_t status;

    if (!parse_http_status (&status, &line))
    {
	log_fatal ("Server returned malformed status line");
    }

    if (status != 200)
    {
	log_fatal ("Server returned error status %zu", status);
    }

    interface->read = http_get_header_func;

    return true;
    
fail:
    *error = true;
    return false;
}

static bool get_request (int fd, const char * host, const char * port, const char * path)
{
    assert (host);
    assert (*host);
    return 0 < dprintf (fd,
			"GET /%s HTTP/" HTTP_VERSION "\n" "Host: %s:%s\n" "User-Agent: http-lib/0.0.1\n" "Accept: */*\n" "\n",
			path,
			host,
			port);
}

convert_interface * http_client_get (http_client * client, const char * path)
{
    if (!get_request (client->read.fd, client->host, client->port, path))
    {
	log_fatal ("Failed to message server");
    }

    convert_interface_http_get * retval = calloc (1, sizeof (*retval));

    retval->client = client;
    retval->interface.read = http_get_status_func;
    
    return (convert_interface*) retval;

fail:
    return NULL;
}

http_client * http_client_connect (const char * host, const char * port)
{
    int fd = tcp_connect (host, port);

    if (fd < 0)
    {
	perror ("tcp_connect");
	return NULL;
    }
    
    http_client * retval = calloc (1, sizeof (*retval));

    retval->read = convert_interface_fd_init(fd);

    retval->host = strdup (host);
    retval->port = strdup (port);

    return retval;
}

void http_client_disconnect (http_client * client)
{
    free (client->host);
    free (client->port);
    free (client);
}

void http_get_free (convert_interface * interface)
{
    free (interface);
}
