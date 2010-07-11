/*===--- cmusic.c - RESTful music information api -----------------------===//
 *
 * Copyright 2010 Bobby Powers
 *
 * This file is part of cfunc.
 *
 * cfunc is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * cfunc is distributed in the hope that it will be useful, but WITHOUT
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cfunc.  If not, see <http://www.gnu.org/licenses/>.
 *
 *===--------------------------------------------------------------------===//
 *
 * The main dispatch for the RESTful API
 *
 *===--------------------------------------------------------------------===//
 */
#include <cfunc/cfunc.h>
#include <cfunc/common.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include <pthread.h>
#include <time.h>

#include <json-glib/json-glib.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>

#include <postgresql/libpq-fe.h>


static uint16_t DEFAULT_PORT = 1969;
static const char DEFAULT_ADDR[] = "127.0.0.1";
static const char CONN_INFO[] = "dbname = cmusic";

// should be stuck into a config.h eventually
const char CANONICAL_NAME[] = "cmusic";
const char PACKAGE[] = "cmusic";
const char VERSION[] = "0.0.1";
const char YEAR[] = "2010";
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

static void print_help(void);
static void print_version(void);


typedef void(*evhttp_cb)(struct evhttp_request *, void *);


/*
static void *
cfunc_car(struct cfunc_cons *coll)
{
	return coll->ops->car(coll);
}

struct cfunc_cons *
cfunc_cdr(struct cfunc_cons *coll)
{
	return coll->ops->cdr(coll);
}

struct cfunc_cons *
cfunc_map(cfunc_closure_t f, struct cfunc_cons *coll)
{
	void *a = cfunc_car(coll);
	struct cfunc_cons *c = coll;
	for (; a != NULL; a = c->car, c=c->cdr)
	{
		f(a);
	}
	return 0;
}
*/


static void
free_cb(const void *data, size_t datalen, void *extra)
{
	g_free((gpointer)data);
}


static inline PGresult *
pg_exec(PGconn *conn, const char *query)
{
	PGresult *res;
	ExecStatusType status;

	res = PQexec(conn, query);
	status = PQresultStatus(res);
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "'%s' command failed (%d): %s", query,
			status, PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		exit_msg("");
	}

	return res;
}


static char *
query_list(const char *query_fmt)
{
	PGconn *conn;
	PGresult *res;
	int rows;
	gsize len;
	JsonGenerator *gen;
	JsonArray *arr;
	JsonNode *node;
	char *result;

	conn = PQconnectdb(CONN_INFO);
	if (PQstatus(conn) != CONNECTION_OK) {
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

	res = pg_exec(conn, query_fmt);
	rows = PQntuples(res);

	arr = json_array_sized_new(rows);

	for (int i = 0; i < rows; ++i) {
		JsonNode *n;
		char *val;
		val = PQgetvalue(res, i, 0);
		n = json_node_new(JSON_NODE_VALUE);
		json_node_set_string(n, val);
		json_array_add_element(arr, n);
	}

	node = json_node_new(JSON_NODE_ARRAY);
	json_node_set_array(node, arr);
	json_array_unref(arr);

	gen = json_generator_new();
	json_generator_set_root(gen, node);
	json_node_free(node);

	result = json_generator_to_data(gen, &len);
	g_object_unref(gen);

	PQclear(res);
	PQfinish(conn);

	return result;
}


static void
artist_list(struct evhttp_request *hreq, struct ccgi_state *state)
{
	char *result;
	struct evbuffer *buf;

	//fprintf(stderr, "INFO: artist list\n");

	result = query_list("SELECT DISTINCT artist FROM music ORDER BY artist");

	buf = evbuffer_new();
	if (!buf)
		exit_perr("%s: evbuffer_new", __func__);
	evbuffer_add_reference(buf, result, strlen(result), free_cb, NULL);

	evhttp_send_reply(hreq, HTTP_OK, "OK", buf);
	evbuffer_free(buf);
}


static void
artist_query(struct evhttp_request *hreq, struct ccgi_state *state,
	     const char *artist)
{
	PGconn *conn;
	PGresult *res;
	ExecStatusType status;
	int rows;
	gsize len;
	JsonGenerator *gen;
	JsonArray *arr;
	JsonNode *node;
	char *result;
	const char *query_args[1], *query_fmt;
	struct evbuffer *buf;

	//fprintf(stderr, "INFO: artist query\n");

	query_fmt = "SELECT title, artist, album, track, path"
		    "    FROM music WHERE artist = $1"
		    "    ORDER BY album, track, title";

	conn = PQconnectdb(CONN_INFO);
	if (PQstatus(conn) != CONNECTION_OK) {
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

	// FIXME: sanitize artist
	query_args[0] = artist;
	res = PQexecParams(conn, query_fmt, 1, NULL, query_args, NULL,
			   NULL, 0);
	status = PQresultStatus(res);      
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "'%s' command failed (%d): %s", query_fmt,
			status, PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		exit_msg("");
	}
	rows = PQntuples(res);

	arr = json_array_sized_new(rows);

	for (int i = 0; i < rows; ++i) {
		char *title, *artist, *album, *track_s, *path;
		JsonObject *obj = json_object_new();

		title = PQgetvalue(res, i, 0);
		artist = PQgetvalue(res, i, 1);
		album = PQgetvalue(res, i, 2);
		track_s = PQgetvalue(res, i, 3);
		path = PQgetvalue(res, i, 4);

		json_object_set_string_member(obj, "title", title);
		json_object_set_string_member(obj, "artist", artist);
		json_object_set_string_member(obj, "album", album);
		json_object_set_int_member(obj, "track", atoi(track_s));
		json_object_set_string_member(obj, "path", path);

		json_array_add_object_element(arr, obj);
	}

	node = json_node_new(JSON_NODE_ARRAY);
	json_node_set_array(node, arr);
	json_array_unref(arr);

	gen = json_generator_new();
	json_generator_set_root(gen, node);
	json_node_free(node);

	result = json_generator_to_data(gen, &len);
	g_object_unref(gen);

	buf = evbuffer_new();
	if (!buf)
		exit_perr("%s: evbuffer_new", __func__);
	evbuffer_add_reference(buf, result, strlen(result), free_cb, NULL);
	evhttp_send_reply(hreq, HTTP_OK, "OK", buf);
	evbuffer_free(buf);

	PQclear(res);
	PQfinish(conn);
}


static void
handle_artist(struct evhttp_request *req, struct ccgi_state *state)
{
	char *artist;
	const char*request_path;

	if (unlikely (!state))
		exit_msg("%s: null state", __func__);

	evhttp_add_header(req->output_headers, "Content-Type",
			  "application/json; charset=UTF-8");

	request_path = strchr(&evhttp_request_get_uri(req)[1], '/');

	if (!request_path || !strcmp(request_path, "/")) {
		artist_list(req, state);
	}
	else {
		artist = g_uri_unescape_string(&request_path[1], NULL);
		artist_query(req, state, artist);
		free(artist);
	}
}


static void
handle_album(struct evhttp_request *req, struct ccgi_state *state)
{
	char *result;
	struct evbuffer *buf;

	if (unlikely (!state))
		exit_msg("%s: null state", __func__);

	evhttp_add_header(req->output_headers, "Content-Type",
			  "application/json; charset=UTF-8");

	//fprintf(stderr, "INFO: album list\n");

	result = query_list("SELECT DISTINCT album FROM music ORDER BY album");

	buf = evbuffer_new();
	if (!buf)
		exit_perr("%s: evbuffer_new", __func__);
	evbuffer_add_reference(buf, result, strlen(result), free_cb, NULL);
	evhttp_send_reply(req, HTTP_OK, "OK", buf);
	evbuffer_free(buf);
}


static void
handle_generic(struct evhttp_request *req, struct ccgi_state *state)
{
	const char*request_path;
	struct evbuffer *buf;

	buf = evbuffer_new();
	if (!buf)
		exit_perr("%s: evbuffer_new", __func__);

	evhttp_add_header(req->output_headers, "Content-Type",
			  "application/json; charset=UTF-8");

	request_path = evhttp_request_get_uri(req);

	evbuffer_add_printf(buf, "\"bad request path '%s'\"", request_path);
	evhttp_send_reply(req, HTTP_OK, "not found", buf);
	evbuffer_free(buf);
}


int
main(int argc, char *const argv[])
{
	int optc, err;
	uint16_t port;
	const char *addr;
	struct ccgi_state state;
	struct sigaction sa = {.sa_handler = SIG_IGN};

	/* Make stderr line buffered - we only use it for debug info */
	setvbuf(stderr, NULL, _IOLBF, 0);

	program_name = argv[0];
	addr = DEFAULT_ADDR;
	port = DEFAULT_PORT;

	// default
	state.mount_point = "/api";

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

	g_type_init();

	// automatically reap zombies
	sigaction(SIGCHLD, &sa, NULL);

	state.ev_base = event_base_new();
	if (!state.ev_base)
		exit_perr("main: event_base_new");

	state.ev_http = evhttp_new(state.ev_base);
	if (!state.ev_http)
		exit_perr("main: evhttp_new");

	err = evhttp_bind_socket(state.ev_http, addr, port);
	if (err)
		exit_msg("main: couldn't bind to %s:%d", addr, port);

	evhttp_set_gencb(state.ev_http, (evhttp_cb)handle_generic, &state);
	evhttp_set_cb(state.ev_http, "/artist*", (evhttp_cb)handle_artist, &state);
	evhttp_set_cb(state.ev_http, "/album*", (evhttp_cb)handle_album, &state);

	fprintf(stderr, "%s: initialized and waiting for connections\n",
		program_name);

	// the main event loop
	event_base_dispatch(state.ev_base);

	return 0;
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
