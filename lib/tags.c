/*===--- tags.c - tag manipulation functions ----------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#include "common.h"
#include "tags.h"
#include "utils.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ftw.h>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include <time.h>
#include <sys/stat.h>

#include <stdint.h>

#include <sqlite3.h>
#include <taglib/tag_c.h>

static const char CREATE_STMT[] =
	"CREATE TABLE IF NOT EXISTS music ("
	"       path     varchar(512) PRIMARY KEY NOT NULL,"
	"       title    varchar(256) NOT NULL,"
	"       artist   varchar(256) NOT NULL,"
	"       album    varchar(256) NOT NULL,"
	"       track    int,"
	"       time     int,"
	"       modified int64"
	")";

static const char INSERT_QUERY[] =
	"INSERT INTO music (path, title, artist, album, track, time, modified)"
	"    VALUES (?, ?, ?, ?, ?, ?, ?)";

static const char UPDATE_QUERY[] =
	"UPDATE music SET title = ?, artist = ?, album = ?, track = ?,"
	"                 time = ?, modified = ?"
	"    WHERE path = ?";

static const char MODIFIED_QUERY[] =
	"SELECT modified"
	"    FROM music WHERE path = ?";

// so we can keep track of our db
struct db_info {
	sqlite3 *db;
	sqlite3_stmt *insert_query;
	sqlite3_stmt *update_query;
	sqlite3_stmt *modified_query;
};

#define PREPARE_QUERY(in, out) do {					\
		err = sqlite3_prepare_v2(db,				\
					 in,				\
					 strlen(in),			\
					 out,				\
					 NULL);				\
		if (err != SQLITE_OK)					\
			exit_msg("sqlite3 prepare '%s' error: %d", in, err); \
	} while (false)


void *tags_init(const char *db_path)
{
	int err;
	sqlite3 *db;
	struct db_info *ret;
	char *err_msg;

	db = NULL;
	ret = xcalloc(sizeof(struct db_info));

	if (sqlite3_threadsafe() == 0)
		exit_msg("sqlite3 not configured to be thread safe, exiting");

	err = sqlite3_open(db_path, &db);
	if (err != SQLITE_OK)
		exit_msg("sqlite3 open error: %d", err);
	ret->db = db;

	err = sqlite3_exec(db, CREATE_STMT, NULL, NULL, &err_msg);
	if (err != SQLITE_OK)
		exit_msg("sqlite3 create error: %d", err);

	PREPARE_QUERY(INSERT_QUERY, &ret->insert_query);
	PREPARE_QUERY(UPDATE_QUERY, &ret->update_query);
	PREPARE_QUERY(MODIFIED_QUERY, &ret->modified_query);

	return ret;
}

void cleanup_cb(struct dirwatch *self)
{
	struct db_info *dbi = self->data;
	sqlite3_finalize(dbi->insert_query);
	sqlite3_finalize(dbi->update_query);
	sqlite3_finalize(dbi->modified_query);
	sqlite3_close(dbi->db);
}

/**
 * In the future we'll want to match on file content, but for now
 * simply matching on extension is fine.
 */
bool
is_valid_cb(struct dirwatch *self __unused__, const char *path __unused__,
	    const char *dir __unused__, const char *file)
{
	char *ext;
	ext = strrchr(file, '.');
	if (!ext)
		return false;
	ext++;

	if (strcmp(ext, "mp3") == 0 ||
	    strcmp(ext, "m4a") == 0 ||
	    strcmp(ext, "ogg") == 0 ||
	    strcmp(ext, "flac") == 0)
		return true;
	return false;
}

static int64_t
get_last_mtime(sqlite3_stmt *modified_stmt, const char *path)
{
	int64_t mtime;
	int err;

	err = sqlite3_bind_text(
		modified_stmt,
		1,
		path,
		strlen(path),
		SQLITE_STATIC);
	if (err != SQLITE_OK)
		exit_msg("sqlite3_bind: %d\n", err);

	// if we don't have a record, and its a valid file, then its new
	// and by definition modified
	err = sqlite3_step(modified_stmt);
	if (err == SQLITE_ROW)
		mtime = sqlite3_column_int64(modified_stmt, 0);
	else
		mtime = -1L;
		

	sqlite3_reset(modified_stmt);
	sqlite3_clear_bindings(modified_stmt);

	return mtime;
}

bool
is_modified_cb(struct dirwatch *self,
	       const char *path,
	       const char *dir __unused__,
	       const char *file __unused__)
{
	struct db_info *dbi;
	int64_t last_time;
	const char *rel_path;;
	bool ret;
	sqlite3 *db;
	struct stat stats;

	dbi = self->data;
	db = dbi->db;

	// rel path is the path under '$dir_name/'
	rel_path = &path[strlen(self->dir_name) + 1];

	stat(path, &stats);

	last_time = get_last_mtime(dbi->modified_query, rel_path);
	ret = stats.st_mtime > last_time;

	// if we have rows in the result, it exists.
	return ret;
}

static bool
song_exists(struct db_info *dbi, const char *const short_path)
{
	return get_last_mtime(dbi->modified_query, short_path) != -1L;
}

void
process_file(const char *full_path, const char *rel_path, struct db_info *dbi)
{
	TagLib_File *file;
	TagLib_Tag *tag;
	int err;
	const TagLib_AudioProperties *props;
	const char *text;
	struct stat stats;
	sqlite3_stmt *stmt;

	if (song_exists(dbi, rel_path)) {
		//printf("  changed\n");
		fflush(stdout);
		stmt = dbi->update_query;
	} else {
		//printf("  new '%s'\n", rel_path);
		fflush(stdout);
		stmt = dbi->insert_query;
	}

	file = taglib_file_new(full_path);
	if (file == NULL) {
		fprintf(stderr,
			"%s: WARNING: couldn't open '%s' for reading id3 tags.\n",
			program_name, full_path);
		return;
	}
	tag = taglib_file_tag(file);
	props = taglib_file_audioproperties(file);
	if (tag == NULL || props == NULL) {
		fprintf(stderr,
			"%s: WARNING: couldn't open '%s's tags or props.\n",
			program_name, full_path);
		return;
	}

	stat(full_path, &stats);

	sqlite3_bind_text(stmt, 1, rel_path, strlen(rel_path), SQLITE_STATIC);
	text = taglib_tag_title(tag);
	sqlite3_bind_text(stmt, 2, text, strlen(text), SQLITE_STATIC);
	text = taglib_tag_artist(tag);
	sqlite3_bind_text(stmt, 3, text, strlen(text), SQLITE_STATIC);
	text = taglib_tag_album(tag);
	sqlite3_bind_text(stmt, 4, text, strlen(text), SQLITE_STATIC);
	sqlite3_bind_int(stmt, 5, taglib_tag_track(tag));
	sqlite3_bind_int(stmt, 6, taglib_audioproperties_length(props));
	sqlite3_bind_int64(stmt, 7, stats.st_mtime);

	err = sqlite3_step(stmt);
	if (err != SQLITE_DONE)
		exit_msg("command failed: %d - %s (%s)", err, sqlite3_errmsg(dbi->db));

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	// free the generated sha256 hex string
	//free((void *)query_args[6]);

	taglib_tag_free_strings();
	taglib_file_free(file);
}

void
change_cb(struct dirwatch *self, const char *path,
	  const char *dir __unused__, const char *file __unused__)
{
	struct db_info *dbi;
	const char *rel_path;

	dbi = self->data;

	// rel path is the path under '$dir_name/'
	rel_path = &path[strlen(self->dir_name) + 1];

	process_file(path, rel_path, dbi);
}

void
delete_cb(struct dirwatch *self, const char *path,
	  const char *dir __unused__, const char *file __unused__)
{
/*
	PGconn *conn;
	PGresult *res;
	ExecStatusType status;
	const char *query_args[1], *rel_path;

	conn = self->data;

	// rel path is the path under '$dir_name/'
	rel_path = &path[strlen(self->dir_name) + 1];
	printf("  deleting: '%s'\n", rel_path);
	fflush(stdout);

	static const char *const query_fmt = "DELETE FROM music "
		"    WHERE path = $1";

	if (PQstatus(conn) != CONNECTION_OK) {
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}
	pg_exec(conn, "BEGIN");

	query_args[0] = rel_path;
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

	pg_exec(conn, "END");
	PQclear(res);
*/
}
