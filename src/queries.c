#include "queries.h"
#include "common.h"
#include "utils.h"
#include "list.h"
#include "db.h"

#include <stdlib.h>

#include <glib.h>

#define ALLOWED_CHARS " \t\r\n'/()!,*&#:"

static char *query_list(sqlite3 *conn, const char *query_fmt);
static char *song_query(sqlite3 *conn, const char *query_fmt, const char *name);

static char *artist_list(struct req *self);
static char *artist_query(struct req *self, const char *artist);
static char *album_list(struct req *self);
static char *album_query(struct req *self, const char *artist);

struct ops artist_ops = {
	.list = artist_list,
	.query = artist_query,
};

struct ops album_ops = {
	.list = album_list,
	.query = album_query,
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
	struct info *ret = xcalloc(sizeof(struct info) + sizeof(struct song));
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
	struct info *ret = xcalloc(sizeof(struct info));

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


static void
info_list_destroy(struct list_head *head)
{
	struct list_head *pos;
	for (pos = head->next; pos != head;) {
		struct info *curr;
		curr = container_of(pos, struct info, list);
		// get next pointer now since we're freeing the memory
		pos = pos->next;
		info_free(curr);
	}
}


static char *
artist_list(struct req *self)
{
	return query_list(
		self->db,
		"SELECT DISTINCT artist FROM music");
}


static char *
artist_query(struct req *self, const char *artist)
{
	static const char *query_fmt =
		"SELECT title, artist, album, track, path"
		"    FROM music WHERE artist = ?"
		"    ORDER BY album, track, title";

	return song_query(self->db, query_fmt, artist);
}


static char *
album_list(struct req *self)
{
	return query_list(
		self->db,
		"SELECT DISTINCT album FROM music");
}


static char *
album_query(struct req *self, const char *album)
{
	static const char *query_fmt =
		"SELECT title, artist, album, track, path"
		"    FROM music WHERE album = ?"
		"    ORDER BY album, track, title";
	return song_query(self->db, query_fmt, album);
}

// returns a string containing a JSON representation of the data
// returned by the particular query passed in.  In this case, its
// always a (JSON) list of (quoted) strings.  Its used by both the
// artist_list and album_list functions.
static char *
query_list(sqlite3 *db, const char *query_fmt)
{
	int len, err;
	char *result;
	sqlite3_stmt *stmt;

	LIST_HEAD(list);
	PREPARE_QUERY(db, query_fmt, &stmt);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		struct info *row;
		const char *val;

		// we don't care about the unsigned qualifier
		val = (const char *)sqlite3_column_text(stmt, 0);
		row = info_string_new(val);
		list_add(&list, &row->list);
	}

	// the +1 is for the trailing null byte.
	len = list_length(&list) + 1;
	result = xcalloc(len);
	list_jsonify(&list, result);

	info_list_destroy(&list);

	sqlite3_finalize(stmt);

	return result;
}


static char *
song_query(sqlite3 *db, const char *query_fmt, const char *name)
{
	int len, err;
	char *result;
	sqlite3_stmt *stmt;

	LIST_HEAD(list);
	PREPARE_QUERY(db, query_fmt, &stmt);

	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		struct info *row;
		const char *title, *artist, *album, *track, *path;

		// we don't care about the unsigned qualifier
		title = (const char *)sqlite3_column_text(stmt, 0);
		artist = (const char *)sqlite3_column_text(stmt, 1);
		album = (const char *)sqlite3_column_text(stmt, 2);
		track = (const char *)sqlite3_column_text(stmt, 3);
		path = (const char *)sqlite3_column_text(stmt, 4);
		row = info_song_new(title, artist, album, track, path);
		list_add(&list, &row->list);
	}

	// the +1 is for the trailing null byte.
	len = list_length(&list) + 1;
	result = xcalloc(len);
	list_jsonify(&list, result);

	info_list_destroy(&list);

	sqlite3_finalize(stmt);

	return result;
}
