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

#include <sqlite3.h>
#include <taglib/tag_c.h>

static const char INSERT_QUERY[] =
	"INSERT INTO music (title, artist, album, track, time, path, hash, modified)"
	"    VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";

static const char UPDATE_QUERY[] =
	"UPDATE music SET title = $1, artist = $2, album = $3, track = $4,"
	"                 time = $5, hash = $7, modified = $8"
	"    WHERE path = $6";

static const char MODIFIED_QUERY[] =
	"SELECT modified"
	"    FROM music WHERE path = $1";

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
					 strlen(in) + 1,		\
					 out,				\
					 NULL);				\
		if (err != SQLITE_OK)					\
			exit_msg("sqlite3 prepare 1 error: %d", err);	\
	} while (false)


void *tags_init(const char *db_path)
{
	int err;
	sqlite3 *db = NULL;
	struct db_info *ret;

	ret = xmalloc(sizeof(struct db_info));

	err = sqlite3_open(db_path, &db);
	if (err != SQLITE_OK)
		exit_msg("sqlite3 open error: %d", err);
	ret->db = db;

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

bool
is_modified_cb(struct dirwatch *self, const char *path,
	       const char *dir __unused__,
	       const char *file __unused__)
{
	PGconn *conn;
	PGresult *res;
	ExecStatusType status;
	int rows;
	char mtime[26], *last_time;
	const char *query_args[1], *rel_path;;
	bool ret;
	struct stat stats;

	conn = self->data;

	// rel path is the path under '$dir_name/'
	rel_path = &path[strlen(self->dir_name) + 1];

	// default to true...
	ret = true;

	stat(path, &stats);
	ctime_r(&stats.st_mtime, mtime);
	// remove the trailing newline
	mtime[strlen(mtime)-1] = '\0';

	static const char *const query_fmt = EXISTS_FMT;

	if (PQstatus(conn) != CONNECTION_OK) {
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

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

	// if we don't have a record, and its a valid file, then its new
	// and by definition modified
	rows = PQntuples(res);
	if (rows == 0)
		return true;

	last_time = PQgetvalue(res, 0, 0);
	if (strcmp(mtime, last_time))
		ret = false;

	PQclear(res);

	if (rows > 1)
		fprintf(stderr, "more than 1 song registered for path '%s'\n",
			rel_path);

	// if we have rows in the result, it exists.
	return ret;
}

static bool
song_exists(PGconn *conn, const char *const short_path)
{
	PGresult *res;
	ExecStatusType status;
	int rows;
	const char *query_args[1];

	static const char *const query_fmt = "SELECT hash"
		"    FROM music WHERE path = $1";

	if (PQstatus(conn) != CONNECTION_OK) {
		PQclear(res);
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

	query_args[0] = short_path;
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

	PQclear(res);

	if (rows > 1)
		fprintf(stderr, "more than 1 song registered for path '%s'\n",
			short_path);

	// if we have rows in the result, it exists.
	return rows > 0;
}

void
process_file(const char *full_path, const char *rel_path, PGconn *conn)
{
	TagLib_File *file;
	TagLib_Tag *tag;
	PGresult *res;
	const TagLib_AudioProperties *props;
	char mtime[26];
	struct stat stats;
	const char *query_args[8];
	static const char *query_fmt;

	if (song_exists(conn, rel_path)) {
		printf("  changed\n");
		fflush(stdout);
		query_fmt = UPDATE_SONG;
	} else {
		printf("  new '%s'\n", rel_path);
		fflush(stdout);
		query_fmt = INSERT_SONG;
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
	ctime_r(&stats.st_mtime, mtime);
	// remove the trailing newline
	mtime[strlen(mtime)-1] = '\0';

	query_args[0] = taglib_tag_title(tag);
	query_args[1] = taglib_tag_artist(tag);
	query_args[2] = taglib_tag_album(tag);
	asprintf((char **)&query_args[3], "%d", taglib_tag_track(tag));
	asprintf((char **)&query_args[4], "%d",
		 taglib_audioproperties_length(props));
	query_args[5] = rel_path;
	query_args[6] = sha256_hex_file(full_path, stats.st_size);
	query_args[7] = mtime;

	res = PQexecParams(conn, query_fmt, 8, NULL,
			   query_args, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "'%s' command failed: %s", query_fmt,
			PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		exit_msg("");
	}

	// free the generated sha256 hex string
	//free((void *)query_args[6]);

	taglib_tag_free_strings();
	taglib_file_free(file);
}

void
change_cb(struct dirwatch *self, const char *path,
	  const char *dir __unused__, const char *file __unused__)
{
	const char *rel_path;
	PGconn *conn;

	conn = self->data;
	// rel path is the path under '$dir_name/'
	rel_path = &path[strlen(self->dir_name) + 1];

	if (PQstatus(conn) != CONNECTION_OK) {
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}
	pg_exec(conn, "BEGIN");
	process_file(path, rel_path, conn);
	pg_exec(conn, "END");
}

void
delete_cb(struct dirwatch *self, const char *path,
	  const char *dir __unused__, const char *file __unused__)
{
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
}
