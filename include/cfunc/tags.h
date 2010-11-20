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

struct watch_state;
struct inotify_event;
struct watch_ops {
	void (*on_change)(struct watch_state *self, struct inotify_event *i);
	void (*_free)(struct watch_state *self);
};
extern const struct watch_ops default_watch_ops;

struct watch_state {
	const struct watch_ops *ops;
	const char *dir_name;
	GHashTable *wds;
	PGconn *conn;
	int iflags;
	int ifd;
};

struct watch_state *watch_state_new(const char *dir_name, int iflags,
				    PGconn *conn);

void *watch_routine(struct watch_state *self);
void process_file(const char *file_path, const char *base_path, PGconn *conn);

#endif // _TAGS_H_
