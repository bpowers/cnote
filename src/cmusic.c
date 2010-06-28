/*===--- exaample.c - Example cfunc program ------------------------------===//
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
 * Just an example of how to use cfunc.
 *
 *===--------------------------------------------------------------------===//
 */
#include <cfunc/cfunc.h>
#include <cfunc/common.h>
#include <cfunc/net.h>

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

#include <postgresql/libpq-fe.h>


static const char DEFAULT_PORT[] = "1969";
static const char DEFAULT_ADDR[] = "localhost";
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


static void
print_headers(struct ccgi_state *state, int http_status)
{
	char *header, *status;

	// FIXME: validate http_status
	switch (http_status) {
	case 404:
		status = "404 Not Found";
	case 200:
	default:
		status = "200 OK";
	}

	asprintf(&header,
		 "Status: %s\r\n"
		 "Content-Type: application/json\r\n\r\n",
		 status);

	xwrite(state->socket, header, strlen(header));
}


static void
artist_list(struct ccgi_state *state)
{
	PGconn *conn;
	PGresult *res;
	int rows;
	gsize len;
	JsonGenerator *gen;
	JsonArray *arr;
	JsonNode *node;
	char *result;

	fprintf(stderr, "INFO: artist list\n");

	print_headers(state, 200);

	conn = PQconnectdb(CONN_INFO);
	if (PQstatus(conn) != CONNECTION_OK) {
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

	res = pg_exec(conn,
		      "SELECT DISTINCT artist FROM music ORDER BY artist");
	rows = PQntuples(res);

	arr = json_array_sized_new(rows);

	for (int i = 0; i < rows; ++i) {
		char *val;
		val = PQgetvalue(res, i, 0);
		json_array_add_string_element(arr, val);
	}

	gen = json_generator_new();
	node = json_node_new(JSON_NODE_ARRAY);
	json_node_set_array(node, arr);
	json_generator_set_root(gen, node);
	result = json_generator_to_data(gen, &len);
	write(state->socket, result, strlen(result));

	json_node_free(node);
	json_array_unref(arr);
	g_object_unref(gen);

	g_free(result);
	PQfinish(conn);
}


static void
artist_query(struct ccgi_state *state, const char *artist)
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

	fprintf(stderr, "INFO: artist query\n");

	query_fmt = "SELECT title, artist, album, track, path"
		    "    FROM music WHERE artist = $1"
		    "    ORDER BY album, track, title";

	print_headers(state, 200);

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

	gen = json_generator_new();
	node = json_node_new(JSON_NODE_ARRAY);
	json_node_set_array(node, arr);
	json_generator_set_root(gen, node);
	result = json_generator_to_data(gen, &len);
	write(state->socket, result, strlen(result));

	json_node_free(node);
	json_array_unref(arr);
	g_object_unref(gen);

	g_free(result);
	PQfinish(conn);
}


/*
static void
print_req_headers(const char *key, const char *val, void *data)
{
	fprintf(stdout, "%s: %s\n", key, val);
}
*/


static void
handle_artist(struct ccgi_state *state, const char *request_path)
{
	char *artist;

	if (unlikely (!state))
		exit_msg("%s: null state", __func__);
	if (unlikely (!request_path))
		exit_msg("%s: null request_path", __func__);

	if (!*request_path || !strcmp(request_path, "/")) {
		artist_list(state);
		return;
	}

	artist = g_uri_unescape_string(&request_path[1], NULL);
	artist_query(state, artist);
	free(artist);
}


static void
handle_request(struct ccgi_state *state)
{
	char *request_path;

	if (unlikely (!state))
		exit_msg("%s: null state", __func__);

	request_path = g_hash_table_lookup(state->headers, "SCRIPT_NAME");
	if (unlikely (!request_path || !g_str_has_prefix(request_path,
							 state->mount_point)))
		exit_msg("%s: awkward... bad request_path", __func__);
	request_path = &request_path[strlen(state->mount_point)+1];

	if (g_str_has_prefix(request_path, "artist"))
		handle_artist(state, &request_path[strlen("artist")]);
	else
		print_headers(state, 404);

	close(state->socket);
}


static void *
new_request(void *data)
{
	struct ccgi_state *state;
	time_t now;

	if (unlikely (!data))
		exit_msg("%s: null data", __func__);
	state = data;

	g_type_init();

	fprintf(stderr, "%s: new thread processing request\n", program_name);

	set_blocking(state->socket);

	state->headers = read_env(state->socket);

	handle_request(state);

	time(&now);
	fprintf(stderr, "%s: done processing request at %s\n", program_name,
		ctime(&now));

	free(state);
	state = NULL;
	data = NULL;

	return NULL;
}


static void
on_accept (int fd, short event, struct ccgi_state *state)
{
	int conn;
	pthread_t tid;
	struct ccgi_state *new_state;

	if (unlikely (!state))
		exit_msg ("%s: null state", __func__);

	conn = accept(state->socket, NULL, NULL);
	if (unlikely(conn < 1)) {
		fprintf (stderr, "%s: accept failed!\n", __func__);
		return;
	}

	new_state = xmalloc0 (sizeof(*new_state));
	new_state->socket = conn;
	new_state->mount_point = state->mount_point;

	pthread_create(&tid, NULL, new_request, new_state);
}


int
main(int argc, char *const argv[])
{
	int optc, err;
	const char *addr, *port;
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
			port = (const char *)optarg;
			break;
		default:
			fprintf(stderr, "unknown option '%c'", optc);
			exit(EXIT_FAILURE);
		}
	}

	// automatically reap zombies
	sigaction(SIGCHLD, &sa, NULL);

	state.socket = get_listen_socket(addr, port);
	state.ev_base = event_base_new();
	if (!state.ev_base)
		exit_perr("main: event_base_new");

	state.ev_curr = event_new(state.ev_base, state.socket,
				  EV_READ|EV_PERSIST,
				  (event_callback_fn)on_accept, &state);
	if (!state.ev_curr)
		exit_perr("main: event_set");

	err = event_add(state.ev_curr, NULL);
	if (err)
		exit_perr("main: event_add");

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
SCGI RESTful access to your music collection.\n\n\
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
