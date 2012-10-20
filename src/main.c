// Copyright 2012 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#include "config.h"

#include "common.h"
#include "queries.h"
#include "dirwatch.h"
#include "tags.h"
#include "utils.h"

#include <pthread.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include <wordexp.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>

#include <glib.h>

static uint16_t DEFAULT_PORT = 1969;
static const char *DEFAULT_ADDR = "127.0.0.1";
static const char *DEFAULT_DIR = "~/Music";
static const char *DEFAULT_DB = "~/.cnote.db";

// global var available to various functions that want to report status
const char *program_name;

static const struct option longopts[] =
{
	{"address", required_argument, NULL, 'a'},
	{"port", required_argument, NULL, 'p'},
	{"dir", required_argument, NULL, 'd'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

static const char *INITIAL_STMTS[] =
{
	"CREATE TABLE IF NOT EXISTS music ("
	"       path     varchar(512) PRIMARY KEY NOT NULL,"
	"       title    varchar(256) NOT NULL,"
	"       artist   varchar(256) NOT NULL,"
	"       album    varchar(256) NOT NULL,"
	"       track    int,"
	"       time     int,"
	"       modified int64"
	")",
	"CREATE INDEX IF NOT EXISTS i_album ON music(album)",
	"CREATE INDEX IF NOT EXISTS i_artist ON music(artist)",
	"CREATE INDEX IF NOT EXISTS i_full ON music(artist, album, title, track, path)",
	NULL
};

// forward declarations
static void print_help(void);
static void print_version(void);

static void handle_unknown(struct evhttp_request *req, void *unused);
static void handle_request(struct evhttp_request *req, struct ops *ops, sqlite3 *db);
static void handle_req(struct evhttp_request *req, sqlite3 *db);

static inline void set_content_type_json(struct evhttp_request *req);

// callback typedef
typedef void (*evhttp_cb)(struct evhttp_request *, void *);

static struct req *
req_new(struct ops *ops, struct evhttp_request *req)
{
	struct req *ret;
	ret = xmalloc(sizeof(*ret));
	ret->ops = ops;
	ret->req = req;
	return ret;
}


//===--- this is where the magic starts... ------------------------------===//
int
main(int argc, char *const argv[])
{
	int optc, err;
	uint16_t port;
	wordexp_t w;
	const char *addr, *dir, *db_path;
	char *err_msg;
	struct dirwatch *watch;

	sqlite3 *db;

	struct evhttp *ev_http;
	struct event_base *ev_base;

	program_name = argv[0];
	addr = DEFAULT_ADDR;
	port = DEFAULT_PORT;
	dir = DEFAULT_DIR;

	// process arguments from the command line
	while ((optc = getopt_long(argc, argv,
				   "a:p:d:hv", longopts, NULL)) != -1) {
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
		case 'd':
			dir = (const char *)optarg;
			break;
		default:
			fprintf(stderr, "unknown option '%c'", optc);
			exit(EXIT_FAILURE);
		}
	}

	// expand any '~' or vars in the music dir path
	err = wordexp(dir, &w, 0);
	if (err)
		exit_perr("main: wordexp");
	dir = strdup(w.we_wordv[0]);
	wordfree(&w);

	err = wordexp(DEFAULT_DB, &w, 0);
	if (err)
		exit_perr("main: wordexp");
	db_path = strdup(w.we_wordv[0]);
	wordfree(&w);

	if (sqlite3_threadsafe() == 0)
		exit_msg("sqlite3 not configured to be thread safe, exiting");

	err = sqlite3_open(db_path, &db);
	if (err != SQLITE_OK)
		exit_msg("couldn't open db");

	for (const char **stmt = INITIAL_STMTS; *stmt; stmt++) {
		err = sqlite3_exec(db, *stmt, NULL, NULL, &err_msg);
		if (err != SQLITE_OK)
			exit_msg("sqlite3 create error: %d (%s)", err, err_msg);
	}

	ev_base = event_base_new();
	if (!ev_base)
		exit_perr("main: event_base_new");

	ev_http = evhttp_new(ev_base);
	if (!ev_http)
		exit_perr("main: evhttp_new");

	err = evhttp_bind_socket(ev_http, addr, port);
	if (err)
		exit_msg("main: couldn't bind to %s:%d", addr, port);

	// set the handlers for the api requests we care about, and set
	// a generic error handler for everything else
	evhttp_set_gencb(ev_http, (evhttp_cb)handle_req, db);

	fprintf(stderr, "%s: initialized and waiting for connections\n",
		program_name);

	watch = dirwatch_new();
	watch->is_valid = is_valid_cb;
	watch->is_modified = is_modified_cb;
	watch->on_delete = delete_cb;
	watch->on_change = change_cb;
	watch->cleanup = cleanup_cb;
	watch->dir_name = dir;
	watch->data = tags_init(db);
	if (watch->data == NULL) {
		exit_msg("%s: couldn't connect to sqlite", program_name);
	}
	dirwatch_init(watch);

	// the main event loop
	event_base_dispatch(ev_base);

	err = sqlite3_close(db);
	if (err != SQLITE_OK)
		exit_msg("close err: %d - %s\n", err,
			 sqlite3_errmsg(db));

	return 0;
}


// called when we get a request like /albums or /album/Album Of The Year
static void
handle_request(struct evhttp_request *req, struct ops *ops, sqlite3 *db)
{
	struct req *request;
	const char *name;
	const char *result;
	struct evbuffer *buf;

	// we always return JSON
	set_content_type_json(req);

	request = req_new(ops, req);
	request->db = db;

	// the URI always starts with a /, so check for another one.
	// if there IS another '/', it means we have a request for a
	// particular artist.
	name = strchr(&evhttp_request_get_uri(req)[1], '/');

	// read as: if we don't have a request name or if the request
	// name is (exactly) the string "/", list the albums, else...
	if (!name || !strcmp(name, "/")) {
		result = request->ops->list(request);
	} else {
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

	printf("bad path '%s'\n", evhttp_request_get_uri(req));
	evbuffer_add_printf(buf, "\"bad request path '%s'\"",
			    evhttp_request_get_uri(req));
	evhttp_send_reply(req, HTTP_OK, "not found", buf);
	evbuffer_free(buf);
}

#define ARTIST "/artist"
#define ALBUM "/album"

// called for each request
static void
handle_req(struct evhttp_request *req, sqlite3 *db)
{
	const char *path;

	path = evhttp_request_get_uri(req);

	if (strncmp(ARTIST, path, strlen(ARTIST)) == 0)
		handle_request(req, &artist_ops, db);
	else if (strncmp(ALBUM, path, strlen(ALBUM)) == 0)
		handle_request(req, &album_ops, db);
	else
		handle_unknown(req, NULL);
}

static void
print_help()
{
	printf("\
Usage: %s [-aphv]\n", program_name);
	printf("\
RESTful access to data about your music collection.\n\n\
Options:\n");
	printf("\n");
	printf("\
  -h, --help          display this help and exit\n\
  -v, --version       display version information and exit\n");
	printf("\n");
	printf("\
  -a, --address=ADDR  address to listen for connections on\n\
                      (default: localhost)\n");
	printf("\
  -p, --port=PORT     port to listen for connections on\n\
                      (default: 1984)\n");
	printf("\
  -d, --dir=DIR       directory where music lives\n\
                      (default: ~/Music)\n");
	printf("\n");
	printf("\
Report bugs to <%s>.\n", PACKAGE_BUGREPORT);
}


static void
print_version (void)
{
	printf("%s (%s) %s\n", CANONICAL_NAME, PACKAGE, VERSION);
	printf("\n");
	printf("\
Copyright (C) %s Bobby Powers.\n\
License MIT: <http://www.opensource.org/licenses/mit-license.php>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n", YEAR);
}

static inline void
set_content_type_json(struct evhttp_request *req)
{
	evhttp_add_header(req->output_headers, "Content-Type",
			  "application/json; charset=UTF-8");
}
