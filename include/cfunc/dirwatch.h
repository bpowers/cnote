/*===--- recursive inotify wrapper --------------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _DIRWATCH_H_
#define _DIRWATCH_H_

#include <stdbool.h>
#include <pthread.h>

struct inotify_event;
struct watch_list;

// basically for internal use
struct watch_list {
	char **slab;
	int len;
};

// is_valid may be null, in which case no filtering of the events will
// be performed
struct dirwatch {
	bool (*is_valid)(struct dirwatch *self,
			 const char *path,
			 const char *dir,
			 const char *file);
	bool (*is_modified)(struct dirwatch *self,
			    const char *path,
			    const char *dir,
			    const char *file);
// don't have on_new yet, probably will require recording a list of
// IN_CREATE'ed files, and checking that list when files are
// IN_CLOSE_WRITE'ed
//	void (*on_new)(struct dirwatch *self,
//		       const char *path,
//		       const char *dir,
//		       const char *file);
	void (*on_delete)(struct dirwatch *self,
			  const char *path,
			  const char *dir,
			  const char *file);
	void (*on_change)(struct dirwatch *self,
			  const char *path,
			  const char *dir,
			  const char *file);
	void (*cleanup)(struct dirwatch *self);
	const char *dir_name;
	struct watch_list wds;
	pthread_t tinfo;
	int iflags;
	int ifd;
	void *data;
};

typedef void (*watch_change_cb)(struct dirwatch *self,
                                struct inotify_event *i);

struct dirwatch *dirwatch_new();
// will spawn a new pthread which calls the appropriate callbacks once
// on init and then whenever files are created/changed/deleted.  only
// gives callbacks for files, not directories right now (because thats
// all I need)
int dirwatch_init(struct dirwatch *self);
void dirwatch_free(struct dirwatch *self);

#endif // _DIRWATCH_H_
