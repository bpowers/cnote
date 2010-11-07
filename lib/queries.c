#include <cfunc/queries.h>
#include <cfunc/common.h>

#include <stdlib.h>

#include <json-glib/json-glib.h>
#include <libpq-fe.h>

#define ALLOWED_CHARS " \t\r\n'/()!,*"

static inline PGresult *pg_exec(PGconn *conn, const char *query);
static char *query_list(PGconn *conn, const char *query_fmt);

static char *artist_list(struct req *self);
static char *artist_query(struct req *self, const char *artist);
static char *album_list(struct req *self);
static char *album_query(struct req *self, const char *artist);

static inline char *json_array_to_string(JsonArray *arr);

struct info;
static int info_length(struct info *self);
static int jsonify(struct info *self, char *buf);

static int list_length(struct list_head *self);
static int list_jsonify(struct list_head *self, char *buf);

struct ops artist_ops = {
	.list = artist_list,
	.query = artist_query,
};

struct ops album_ops = {
	.list = album_list,
	.query = album_query,
};


struct json_ops {
	int (*length)(struct info *self);
	int (*jsonify)(struct info *self, char *buf);
};

enum INFO_TYPE {
	STRING,
	SONG
};

struct info {
	struct json_ops *ops;
	struct list_head list;
	enum INFO_TYPE type;
	union {
		char *name;
		struct song *song;
	} data;
};

struct song {
	char *title;
	char *artist;
	char *album;
	char *track;
	char *path;
};


struct json_ops json_ops = {
	.length = info_length,
	.jsonify = jsonify,
};


static struct info *
info_song_new(const char *title, const char *artist, const char *album,
	      const char *track, const char *path)
{
	// in 1 allocation get both info and song, one after another.
	struct info *ret = xmalloc0(sizeof(struct info) + sizeof(struct song));
	struct song *song = (struct song *)&ret[1];

	ret->ops = &json_ops;
	ret->type = SONG;
	ret->data.song = song;

	song->title = g_uri_escape_string(title, ALLOWED_CHARS, true);
	song->artist = g_uri_escape_string(artist, ALLOWED_CHARS, true);
	song->album = g_uri_escape_string(album, ALLOWED_CHARS, true);
	song->track = g_uri_escape_string(track, ALLOWED_CHARS, true);
	song->path = g_uri_escape_string(path, ALLOWED_CHARS, true);

	return ret;
}

static struct info *
info_string_new(const char *name)
{
	// in 1 allocation get both info and song, one after another.
	struct info *ret = xmalloc0(sizeof(struct info));

	ret->ops = &json_ops;
	ret->type = STRING;
	ret->data.name = g_uri_escape_string(name, ALLOWED_CHARS, true);

	return ret;
}

static void
info_free(struct info *self)
{
	if (self->type == STRING) {
		free(self->data.name);
		free(self);
		return;
	}

	free(self->data.song->title);
	free(self->data.song->artist);
	free(self->data.song->album);
	free(self->data.song->track);
	free(self->data.song->path);
	free(self);
}


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
		PQclear(res);
		PQfinish(conn);
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
		PQfinish(conn);
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

		struct info *song = info_song_new(title, artist, album,
						  track_s, path);
		int len = song->ops->length(song);
		char *jsonified = xmalloc0(len);
		song->ops->jsonify(song, jsonified);
		fprintf(stderr, "jsonified: %s\n", jsonified);
		free(jsonified);
		info_free(song);

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
		PQclear(res);
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

		struct info *song = info_song_new(title, artist, album,
						  track_s, path);
		int len = song->ops->length(song);
		char *jsonified = xmalloc0(len);
		song->ops->jsonify(song, jsonified);
		fprintf(stderr, "jsonified: %s\n", jsonified);
		free(jsonified);
		info_free(song);

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
		PQfinish(conn);
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
	int rows, len;
	struct list_head *list;
	char *result;

	list = NULL;

	if (PQstatus(conn) != CONNECTION_OK) {
		PQclear(res);
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

	res = pg_exec(conn, query_fmt);
	rows = PQntuples(res);

	for (int i = 0; i < rows; ++i) {
		struct info *row;
		char *val;
		val = PQgetvalue(res, i, 0);

		row = info_string_new(val);
		list_add(&list, &row->list);
	}

	len = list_length(list);
	result = xmalloc0(len);
	list_jsonify(list, result);

	PQclear(res);

	return result;
}

// opening and closing square brackets
static const int list_overhead = 2;

static int
list_length(struct list_head *self)
{
	int len;
	struct list_head *curr;

	len = list_overhead;

	list_for_each(curr, self) {
		struct info *data = container_of(curr, struct info, list);
		// the +1 is for the trailing comma
		len += data->ops->length(data) + 1;
	}

	// remove the last trailing comma, as per the json.org spec
	len -= 1;

	return len;
}


// opening and closing '{' and '}'
static const int dict_overhead = 2;
// quotes around key and value, ':' and ','
static const int str_overhead = 6;
#define member_len(self, field) \
	(strlen(#field) + strlen(self->data.song->field) + str_overhead)

static int
info_length(struct info *self)
{
	int len;
	// the added two are for matching '"'.
	if (self->type == STRING)
		return strlen(self->data.name) + 2;

	// otherwise we're a dict, or object
	len = dict_overhead;

	// otherwise we're a song;
	len += member_len(self, title);
	len += member_len(self, artist);
	len += member_len(self, album);
	len += member_len(self, track);
	len += member_len(self, path);

	return len;
}

#define member_jsonify(self, field) {				\
	*buf++ = '"';                                           \
	len = strlen(#field);					\
	strncpy(buf, #field, len);				\
	buf += len;						\
	*buf++ = '"';						\
	*buf++ = ':';						\
	*buf++ = '"';						\
	len = strlen(self->data.song->field);			\
	strncpy(buf, self->data.song->field, len);		\
	buf += len;						\
	*buf++ = '"';						\
	*buf++ = ',';						\
}

static int
jsonify(struct info *self, char *buf)
{
	int len;
	if (self->type == STRING) {
		*buf++ = '"';
		len = strlen(self->data.name);
		strncpy(buf, self->data.name, len);
		buf[len] = '"';
		return 0;
	}

	*buf++ = '{';

	member_jsonify(self, title);
	member_jsonify(self, artist);
	member_jsonify(self, album);
	member_jsonify(self, track);
	member_jsonify(self, path);

	*--buf = '}';

	return 0;
}

static int
list_jsonify(struct list_head *self, char *buf)
{
	int len;
	struct list_head *curr;

	*buf++ = '[';

	list_for_each(curr, self) {
		struct info *data = container_of(curr, struct info, list);
		// the +1 is for the trailing comma
		len = data->ops->length(data);
		data->ops->jsonify(data, buf);
		buf += len;
		*buf++ = ',';
	}

	// remove the last trailing comma, replacing it with the closing
	// bracket
	*--buf = ']';

	return len;
}
