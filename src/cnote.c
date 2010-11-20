/*===--- cnote.c - RESTful music information api ------------------------===//
 *
 * Copyright 2010 Bobby Powers
 *
 * This file is part of cnote.
 *
 * cnote is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, version 3 of the License.
 *
 * cnote is distributed in the hope that it will be useful, but WITHOUT
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cnote.  If not, see <http://www.gnu.org/licenses/>.
 *
 *===--------------------------------------------------------------------===//
 *
 * The main dispatch for our RESTful API
 *
 *===--------------------------------------------------------------------===//
 */
//#include <cfunc/cfunc.h>
#include <cfunc/common.h>
#include <cfunc/queries.h>

#include <pthread.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <ftw.h>

#include <sys/inotify.h>
#include <sys/epoll.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>

#include <glib.h>

#include <libpq-fe.h>

PGconn *CONN;

static uint16_t DEFAULT_PORT = 1969;
static const char DEFAULT_ADDR[] = "127.0.0.1";
static const char CONN_INFO[] = "dbname = cmusic";
static const char DEFAULT_DIR[] = "/var/unsecure/music";

// TODO: should be stuck into a config.h eventually
const char CANONICAL_NAME[] = "cnote";
const char PACKAGE[] = "cnote";
const char VERSION[] = "0.0.1";
const char YEAR[] = "2011";
const char PACKAGE_BUGREPORT[] = "bobbypowers@gmail.com";

// global var available to various functions that want to report status
const char *program_name;

static const struct option longopts[] =
{
	{"mount", required_argument, NULL, 'm'},
	{"address", required_argument, NULL, 'a'},
	{"port", required_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

// forward declarations
static void print_help(void);
static void print_version(void);

static void handle_unknown(struct evhttp_request *req, void *unused);
static void handle_request(struct evhttp_request *req, struct ops *ops);

static inline void set_content_type_json(struct evhttp_request *req);

// callback typedef
typedef void (*evhttp_cb)(struct evhttp_request *, void *);


static struct req *
req_new(struct ops *ops, struct evhttp_request *req, PGconn *conn)
{
	struct req *ret;
	ret = xmalloc(sizeof(*ret));
	ret->ops = ops;
	ret->req = req;
	ret->conn = conn;
	return ret;
}

static const int IN_MASK = IN_MODIFY | IN_ATTRIB | IN_MOVE | IN_DELETE | IN_CREATE | IN_DELETE_SELF;
int ifd;

int add_watch(const char *fpath, const struct stat *sb __unused__,
	      int typeflag, struct FTW *ftwbuf __unused__)
{
	int watch;
	// only add watches to directories
	if ((typeflag & FTW_D) == 0)
		return 0;

	watch = inotify_add_watch(ifd, fpath, IN_MASK);
	if (watch == -1)
		exit_perr("inotify error");

	return 0;
}

typedef void *(*pthread_routine)(void *);
static void *
update_routine(const char *dir_name)
{
	struct event_base *ev_base;
	struct epoll_event ev;
	int efd;

	fprintf(stderr, "dir name: %s\n", dir_name);

	ev_base = event_base_new();
	if (!ev_base)
		exit_msg("event_base_new");

	ifd = inotify_init();
	if (ifd < 0)
		exit_perr("inotify_init");

	nftw(dir_name, add_watch, 128, 0);

	efd = epoll_create(1);
	if (efd < 0)
		exit_perr("epoll_create");

	

	pthread_exit(NULL);
}


//===--- this is where the magic starts... ------------------------------===//
int
main(int argc, char *const argv[])
{
	int optc, err;
	uint16_t port;
	const char *addr, *dir;
	struct ccgi_state state;
	pthread_t update_thread;

	CONN = PQconnectdb(CONN_INFO);

	ccgi_state_init(&state);

	program_name = argv[0];
	addr = DEFAULT_ADDR;
	port = DEFAULT_PORT;
	dir = DEFAULT_DIR;

	// process arguments from the command line
	while ((optc = getopt_long(argc, argv,
				   "a:p:hv", longopts, NULL)) != -1) {
		switch (optc) {
		// GNU standards have --help and --version exit immediately.
		case 'v':
			print_version();
			return 0;
		case 'h':
			print_help();
			return 0;
		case 'a':
			addr = (const char *)optarg;
			break;
		case 'p':
			// FIXME: deal with errorz
			port = atoi(optarg);
			break;
		default:
			fprintf(stderr, "unknown option '%c'", optc);
			exit(EXIT_FAILURE);
		}
	}

	state.ev_base = event_base_new();
	if (!state.ev_base)
		exit_perr("main: event_base_new");

	state.ev_http = evhttp_new(state.ev_base);
	if (!state.ev_http)
		exit_perr("main: evhttp_new");

	err = evhttp_bind_socket(state.ev_http, addr, port);
	if (err)
		exit_msg("main: couldn't bind to %s:%d", addr, port);

	// set the handlers for the api requests we care about, and set
	// a generic error handler for everything else
	evhttp_set_gencb(state.ev_http, (evhttp_cb)handle_unknown, NULL);
	evhttp_set_cb(state.ev_http, "/artist*", (evhttp_cb)handle_request, &artist_ops);
	evhttp_set_cb(state.ev_http, "/album*", (evhttp_cb)handle_request, &album_ops);

	fprintf(stderr, "%s: initialized and waiting for connections\n",
		program_name);


	pthread_create(&update_thread, NULL, (pthread_routine)update_routine, (void *)dir);

	// the main event loop
	event_base_dispatch(state.ev_base);

	return 0;
}


// called when we get a request like /albums or /album/Album Of The Year
static void
handle_request(struct evhttp_request *req, struct ops *ops)
{
	struct req *request;
	const char *name;
	const char *result;
	struct evbuffer *buf;

	// we always return JSON
	set_content_type_json(req);

	request = req_new(ops, req, CONN);

	// the URI always starts with a /, so check for another one.
	// if there IS another '/', it means we have a request for a
	// particular artist.
	name = strchr(&evhttp_request_get_uri(req)[1], '/');

	// read as: if we don't have a request name or if the request
	// name is (exactly) the string "/", list the albums, else...
	if (!name || !strcmp(name, "/")) {
		result = request->ops->list(request);
	}
	else {
		char *real_name;
		real_name = g_uri_unescape_string(&name[1], NULL);
		result = request->ops->query(request, real_name);
		free(real_name);
	}

	buf = evbuffer_new();
	if (!buf)
		exit_perr("%s: evbuffer_new", __func__);
	evbuffer_add_reference(buf, result, strlen(result), free_cb, NULL);
	evhttp_send_reply(req, HTTP_OK, "OK", buf);
	evbuffer_free(buf);
}


// called when we don't have an artist or album API call - a fallthrough
// error handler in a sense.
static void
handle_unknown(struct evhttp_request *req, void *unused __unused__)
{
	struct evbuffer *buf;

	buf = evbuffer_new();
	if (!buf)
		exit_perr("%s: evbuffer_new", __func__);

	set_content_type_json(req);

	evbuffer_add_printf(buf, "\"bad request path '%s'\"",
			    evhttp_request_get_uri(req));
	evhttp_send_reply(req, HTTP_OK, "not found", buf);
	evbuffer_free(buf);
}


static void
print_help()
{
	printf ("\
Usage: %s [-aphv]\n", program_name);
	fputs ("\
RESTful access to data about your music collection.\n\n\
Options:\n", stdout);
	puts ("");
	fputs ("\
  -h, --help          display this help and exit\n\
  -v, --version       display version information and exit\n", stdout);
	puts ("");
	fputs ("\
  -a, --address=ADDR  address to listen for connections on\n\
                      (default: localhost)\n", stdout);
	fputs ("\
  -p, --port=PORT     port to listen for connections on\n\
                      (default: 1984)\n", stdout);
	printf ("\n");
	printf ("\
Report bugs to <%s>.\n", PACKAGE_BUGREPORT);
}


static void
print_version (void)
{
	printf ("%s (%s) %s\n", CANONICAL_NAME, PACKAGE, VERSION);
	puts ("");
	// FSF recommends seperating out the year, for ease in translations.
	printf ("\
Copyright (C) %s Bobby Powers.\n\
License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n", YEAR);
}

static inline void
set_content_type_json(struct evhttp_request *req)
{
	evhttp_add_header(req->output_headers, "Content-Type",
			  "application/json; charset=UTF-8");
}
