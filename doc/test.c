#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>

static int count = 1;

static void
handle_generic(struct evhttp_request *req, void *state)
{
	struct evbuffer *buf;

	fprintf(stderr, "INFO: cb %d\n", count++);

	buf = evbuffer_new();
	if (!buf)
		exit(EXIT_FAILURE);

	evbuffer_add_printf(buf, "okay");
	evhttp_send_reply(req, HTTP_OK, "OK", buf);
	evbuffer_free(buf);
}

int
main(int argc, char *const argv[])
{
	struct evhttp *ev_http;
	struct event_base *ev_base;

	ev_base = event_base_new();
	ev_http = evhttp_new(ev_base);
	evhttp_bind_socket(ev_http, "127.0.0.1", 1969);

	evhttp_set_gencb(ev_http, handle_generic, NULL);

	fprintf(stderr, "INFO: listening for connections.\n");
	event_base_dispatch(ev_base);

	return 0;
}
