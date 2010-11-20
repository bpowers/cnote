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

static void handle_ievent(struct watch_state *self, struct inotify_event *i);
static void watch_state_free(struct watch_state *self);

const struct watch_ops default_watch_ops = {
	.on_change = handle_ievent,
	._free = watch_state_free,
};

// nasty global to get around nftw limitations
static int GLOBAL_ifd;
static int GLOBAL_flags;
static GHashTable *GLOBAL_wds;

struct watch_state *
watch_state_new(const char *dir_name, int iflags, PGconn *conn)
{
	struct watch_state *self;

	self = xmalloc0(sizeof(*self));
	self->ops = &default_watch_ops;
	self->dir_name = dir_name;
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
	if (self->wds)
		g_hash_table_unref(self->wds);
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
	if ((typeflag & FTW_D) == 0)
		return 0;

	watch = inotify_add_watch(GLOBAL_ifd, fpath, GLOBAL_flags);
	if (watch == -1)
		exit_perr("inotify error");

	g_hash_table_insert(GLOBAL_wds, (void *)watch, strdup(fpath));

	return 0;
}


// for our watch->dirname hash table
static void
wd_remove_key(gpointer key)
{
	free(key);
}
static void
wd_remove_free(gpointer data)
{
	free(data);
}

static void
handle_ievent(struct watch_state *self, struct inotify_event *i)
{
	printf("    wd =%2d; ", i->wd);
	if (i->cookie > 0)
		printf("cookie =%4d; ", i->cookie);

	printf("mask = ");
	if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
	if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
	if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
	if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
	if (i->mask & IN_CREATE)        printf("IN_CREATE ");
	if (i->mask & IN_DELETE)        printf("IN_DELETE ");
	if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
	if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
	if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");
	if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
	if (i->mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
	if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
	if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
	if (i->mask & IN_OPEN)          printf("IN_OPEN ");
	if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
	if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");
	printf("\n");

        if (i->len > 0)
		printf("        name = %s/%s\n",
		       g_hash_table_lookup(self->wds, (void *)i->wd),
		       i->name);
}

void *
watch_routine(struct watch_state *self)
{
	char buf[IBUF_LEN];
	ssize_t len;

	if (!self)
		exit_msg("update_routine called with null self");

	fprintf(stderr, "dir name: %s\n", self->dir_name);

	self->wds = g_hash_table_new_full(NULL, NULL, wd_remove_key,
					  wd_remove_free);
	if (!self->wds)
		exit_msg("couldn't allocate self->wds");

	self->ifd = inotify_init();
	if (self->ifd == -1)
		exit_perr("inotify_init");

	GLOBAL_ifd = self->ifd;
	GLOBAL_flags = self->iflags;
	GLOBAL_wds = self->wds;
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

			handle_ievent(self, event);

			p += sizeof(*event) + event->len;
		}
	}

	self->ops->_free(self);

	return NULL;
}
