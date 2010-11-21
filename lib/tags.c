/*===--- tags.c - tag manipulation functions ----------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#include <cfunc/common.h>
#include <cfunc/tags.h>

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

#include <taglib/tag_c.h>

#include <libpq-fe.h>

#define IBUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
// the amount of buffer watch descriptors to allocate
#define WD_EXTRA (512)

// TODO: these should be thread local via pthread keys
// nasty globals to get around nftw limitations
static int GLOBAL_ifd;
static int GLOBAL_flags;
static struct watch_list *GLOBAL_wds;
static int GLOBAL_count;


struct watch_state *
watch_state_new(const char *dir_name,
		watch_change_cb callback,
		int iflags,
		PGconn *conn)
{
	struct watch_state *self;

	self = xmalloc0(sizeof(*self));
	self->dir_name = dir_name;
	self->on_change = callback;
	self->iflags = iflags;
	self->conn = conn;

	return self;
}

void
watch_state_free(struct watch_state *self)
{
	// shouldn't be a null pointer passed in.

	if (self->dir_name)
		free((void *)self->dir_name);
	if (self->conn)
		PQfinish(self->conn);
	if (self->wds.slab)
		free(self->wds.slab);
	free(self);
}


void
process_file(const char *file_path, const char *base_path, PGconn *conn)
{
	TagLib_File *file;
	TagLib_Tag *tag;
	PGresult *res;
	const TagLib_AudioProperties *props;
	const char *rel_path;
	char mtime[26];
	struct stat stats;
	size_t len;
	const char *query_args[8];
	static const char query_fmt[] =
		"INSERT INTO music (title, artist, album, track, time, path, hash, modified)"
		"    VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";

	file = taglib_file_new(file_path);
	if (file == NULL) {
		fprintf(stderr,
			"%s: WARNING: couldn't open '%s' for reading id3 tags.\n",
			program_name, file_path);
		return;
	}
	tag = taglib_file_tag(file);
	props = taglib_file_audioproperties(file);
	if (tag == NULL || props == NULL) {
		fprintf(stderr,
			"%s: WARNING: couldn't open '%s's tags or props.\n",
			program_name, file_path);
		return;
	}

	rel_path = file_path;
	if (base_path) {
		len = strlen(base_path);
		if (!strncmp(file_path, base_path, len))
			rel_path = &file_path[len];
	}

	stat(file_path, &stats);
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
	query_args[6] = sha256_hex_file(file_path, stats.st_size);
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
	free((void *)query_args[6]);

	taglib_tag_free_strings();
	taglib_file_free(file);
}


// callback for use with nftw
static int
add_watch(const char *fpath, const struct stat *sb __unused__,
	  int typeflag, struct FTW *ftwbuf __unused__)
{
	int watch;

	// only add watches to directories
	if (!(typeflag & FTW_D))
		return 0;

	watch = inotify_add_watch(GLOBAL_ifd, fpath, GLOBAL_flags);
	if (watch == -1)
		exit_perr("inotify error");

	watch_list_put(GLOBAL_wds, watch, strdup(fpath));

	return 0;
}


// callback for use with nftw
static int
count_dirs(const char *fpath __unused__, const struct stat *sb __unused__,
	  int typeflag, struct FTW *ftwbuf __unused__)
{
	if ((typeflag & FTW_D))
		++GLOBAL_count;
	return 0;
}

void *
watch_routine(struct watch_state *self)
{
	char buf[IBUF_LEN];
	ssize_t len;
	int count;

	if (!self)
		exit_msg("update_routine called with null self");

	fprintf(stderr, "dir name: %s\n", self->dir_name);

	self->ifd = inotify_init();
	if (self->ifd == -1)
		exit_perr("inotify_init");

	GLOBAL_count = 0;
	nftw(self->dir_name, count_dirs, 32, 0);
	count = GLOBAL_count;
	printf("%d dirs\n", GLOBAL_count);

	watch_list_init(&self->wds, count + WD_EXTRA);

	GLOBAL_ifd = self->ifd;
	GLOBAL_flags = self->iflags;
	GLOBAL_wds = &self->wds;
	nftw(self->dir_name, add_watch, 32, 0);
	GLOBAL_ifd = -1;
	GLOBAL_flags = 0;
	GLOBAL_wds = NULL;

	// XXX: for debugging mostly.
	fflush(stdout);

	while (true) {
		len = read(self->ifd, buf, IBUF_LEN);
		if (len == 0)
			exit_msg("inotify returned 0");
		if (len == -1)
			exit_perr("inotify read");

		for (char *p = buf; p < buf + len;) {
			struct inotify_event *event;
			event = (struct inotify_event *)p;

			self->on_change(self, event);

			p += sizeof(*event) + event->len;
		}
	}

	watch_state_free(self);

	return NULL;
}
