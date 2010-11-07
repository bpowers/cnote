#include <cfunc/queries.h>
#include <cfunc/common.h>

#include <stdlib.h>

#include <json-glib/json-glib.h>
#include <libpq-fe.h>

static inline PGresult *pg_exec(PGconn *conn, const char *query);
static char *query_list(PGconn *conn, const char *query_fmt);

static char *artist_list(struct req *self);
static char *artist_query(struct req *self, const char *artist);
static char *album_list(struct req *self);
static char *album_query(struct req *self, const char *artist);

static inline char *json_array_to_string(JsonArray *arr);

struct ops artist_ops = {
	.list = artist_list,
	.query = artist_query,
};

struct ops album_ops = {
	.list = album_list,
	.query = album_query,
};


static char *
artist_list(struct req *self)
{
	return query_list(self->conn, "SELECT DISTINCT artist FROM music ORDER BY artist");
}


static char *
artist_query(struct req *self, const char *artist)
{
	PGconn *conn;
	PGresult *res;
	ExecStatusType status;
	int rows;
	JsonArray *arr;
	char *result;
	const char *query_args[1], *query_fmt;

	query_fmt = "SELECT title, artist, album, track, path"
		    "    FROM music WHERE artist = $1"
		    "    ORDER BY album, track, title";

	conn = self->conn;
	if (PQstatus(conn) != CONNECTION_OK) {
		//PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

	// FIXME: sanitize artist (little bobby tables...)
	query_args[0] = artist;
	res = PQexecParams(conn, query_fmt, 1, NULL, query_args, NULL,
			   NULL, 0);
	status = PQresultStatus(res);      
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "'%s' command failed (%d): %s", query_fmt,
			status, PQerrorMessage(conn));
		PQclear(res);
		//PQfinish(conn);
		exit_msg("");
	}
	rows = PQntuples(res);

	// build a JSON array out of the result
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

	result = json_array_to_string(arr);
	json_array_unref(arr);

	PQclear(res);
	//PQfinish(conn);

	return result;
}


static char *
album_list(struct req *self)
{
	return query_list(self->conn, "SELECT DISTINCT album FROM music ORDER BY album");
}


static char *
album_query(struct req *self, const char *artist)
{
	PGconn *conn;
	PGresult *res;
	ExecStatusType status;
	int rows;
	JsonArray *arr;
	char *result;
	const char *query_args[1], *query_fmt;

	//fprintf(stderr, "INFO: artist query\n");

	query_fmt = "SELECT title, artist, album, track, path"
		    "    FROM music WHERE album = $1"
		    "    ORDER BY album, track, title";

	conn = self->conn;
	if (PQstatus(conn) != CONNECTION_OK) {
		//PQfinish(conn);
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
		//PQfinish(conn);
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

	result = json_array_to_string(arr);
	json_array_unref(arr);

	PQclear(res);
	//PQfinish(conn);

	return result;
}


// wraps simple postgres queries from static strings
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
		//PQfinish(conn);
		exit_msg("");
	}

	return res;
}


static inline char *
json_array_to_string(JsonArray *arr)
{
	gsize len;
	JsonGenerator *gen;
	JsonNode *node;
	char *result;

	node = json_node_new(JSON_NODE_ARRAY);
	json_node_set_array(node, arr);

	gen = json_generator_new();
	json_generator_set_root(gen, node);
	json_node_free(node);

	result = json_generator_to_data(gen, &len);
	g_object_unref(gen);

	return result;
}


// returns a string containing a JSON representation of the data
// returned by the particular query passed in.  In this case, its
// always a (JSON) list of (quoted) strings.  Its used by both the
// artist_list and album_list functions.
static char *
query_list(PGconn *conn, const char *query_fmt)
{
	PGresult *res;
	int rows;
	JsonArray *arr;
	char *result;
;
	if (PQstatus(conn) != CONNECTION_OK) {
		//PQfinish(conn);
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

	result = json_array_to_string(arr);
	json_array_unref(arr);

	PQclear(res);
	//PQfinish(conn);

	return result;
}
