/*===--- tags.h - id3 tag manipulation functions ------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _TAGS_H_
#define _TAGS_H_

#include <glib.h>
#include <libpq-fe.h>

struct inotify_event;

struct watch_state {
	void (*on_change)(struct watch_state *self, struct inotify_event *i);
	const char *dir_name;
	struct watch_list wds;
	PGconn *conn;
	int iflags;
	int ifd;
};

typedef void (*watch_change_cb)(struct watch_state *self,
				struct inotify_event *i);

struct watch_state *watch_state_new(const char *dir_name,
				    watch_change_cb callback,
				    int iflags,
				    PGconn *conn);
void watch_state_free(struct watch_state *self);

void *watch_routine(struct watch_state *self);
void process_file(const char *file_path, const char *base_path, PGconn *conn);
void delete_file(const char *rel_path, PGconn *conn);

#endif // _TAGS_H_
