// Copyright 2012 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#include "common.h"
#include "utils.h"
#include "dirwatch.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/inotify.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ftw.h>

#include <sys/inotify.h>
#include <limits.h>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include <time.h>
#include <sys/stat.h>

#include <alloca.h>


#define IBUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
// the amount of buffer watch descriptors to allocate
#define WD_EXTRA (512)

// don't need IN_DELETE_SELF because we get IN_IGNORED for free
#define IN_MASK	\
	(IN_CLOSE_WRITE | IN_CREATE | IN_MOVE | IN_DELETE)

// TODO: these should be thread local via pthread keys
// nasty globals to get around nftw limitations
static int GLOBAL_ifd;
static int GLOBAL_flags;
static struct watch_list *GLOBAL_wds;
static int GLOBAL_count;
static struct dirwatch *GLOBAL_self;


//static struct watch_list *watch_list_new(void);
static int watch_list_init(struct watch_list *self, int len);
//static void watch_list_free(struct watch_list *self);
static char *watch_list_get(struct watch_list *self, int wd);
static int watch_list_put(struct watch_list *self, int wd, char *path);
static int watch_list_remove(struct watch_list *self, int wd);

static void *watch_routine(struct dirwatch *self);

struct dirwatch *
dirwatch_new()
{
	return xcalloc(sizeof(struct dirwatch));
}

int
dirwatch_init(struct dirwatch *self)
{
	self->iflags = IN_MASK;
	return pthread_create(&self->tinfo, NULL,
			      (pthread_routine)watch_routine, self);
}

void
dirwatch_free(struct dirwatch *self)
{
	// shouldn't be a null pointer passed in.

	if (self->cleanup)
		self->cleanup(self);
	if (self->dir_name)
		free((void *)self->dir_name);
	if (self->wds.slab)
		free(self->wds.slab);
	free(self);
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

// callback for use with nftw
static int
on_startup(const char *fpath , const struct stat *sb __unused__,
	   int typeflag, struct FTW *ftwbuf __unused__)
{
	struct dirwatch *self;
	const char *file;
	char *dir;
	size_t len;

	if (typeflag & FTW_D)
		return 0;

	self = GLOBAL_self;

	// can't fail on 'NIX because every full path should start with a '/'
	file = strrchr(fpath, '/') + 1;
	len = file - fpath;
	// alloca can't fail (but can cause stack overflow...)
	dir = alloca(len);
	memcpy(dir, fpath, len-1);
	// add a terminating null to the alloca'd string.
	dir[len - 1] = '\0';

	// and since we've filtered out events by now that happen to
	// watch descriptors, we're sure that file isn't null (since
	// the struct inotify_event only has a 0 length name field when
	// the event refers to the watch descriptor itself).
	if (self->is_valid && !self->is_valid(self, fpath, dir, file))
		return 0;

	if (self->is_modified(self, fpath, dir, file))
		self->on_change(self, fpath, dir, file);
	return 0;
}

static void
handle_ievent(struct dirwatch *self, struct inotify_event *i)
{
	bool malloced;
	char *full_path, *dir, *file;
	int err;

	dir = watch_list_get(&self->wds, i->wd);
	// get full path
	if (i->len > 0) {
		file = i->name;
		err = asprintf(&full_path, "%s/%s", dir, i->name);
		if (err == -1)
			exit_perr("handle_ievent: asprintf");
		malloced = true;
	} else {
		file = NULL;
		full_path = dir;
		malloced = false;
	}

	//fprintf(stderr, "event (0x%x) for: %s\n", i->mask, full_path);

	// handle new directory creation, or a directory move
	if (i->mask & IN_CREATE ||
	    (i->mask & IN_MOVED_TO && i->mask & IN_ISDIR)) {
		int watch;
		// we only care about create events for directories,
		// because create events for files are followed by
		// IN_CLOSE_WRITE events.
		if (!(i->mask & IN_ISDIR))
			goto cleanup;

		watch = inotify_add_watch(self->ifd, full_path, self->iflags);
		if (watch == -1)
			exit_perr("inotify error");

		watch_list_put(&self->wds, watch, full_path);

		// watch_list now owns name, so don't free it;
		return;
	}

	// handle directory removal
	if (i->mask & (IN_IGNORED | IN_DELETE_SELF) ||
	    (i->mask & IN_MOVED_FROM && i->mask & IN_ISDIR)) {
		watch_list_remove(&self->wds, i->wd);
		goto cleanup;
	}

	// from here on out we only want to deal with files.
	if (i->mask & IN_ISDIR)
		goto cleanup;
	// and since we've filtered out events by now that happen to
	// watch descriptors, we're sure that file isn't null (since
	// the struct inotify_event only has a 0 length name field when
	// the event refers to the watch descriptor itself).
	if (self->is_valid && !self->is_valid(self, full_path, dir, file))
		goto cleanup;

	if (i->mask & IN_DELETE ||
	    i->mask & IN_MOVED_FROM)
		self->on_delete(self, full_path, dir, file);
	else if (self->is_modified(self, full_path, dir, file))
		self->on_change(self, full_path, dir, file);
cleanup:
	if (malloced)
		free(full_path);
}

static void *
watch_routine(struct dirwatch *self)
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

	// find out how many directories we have
	GLOBAL_count = 0;
	nftw(self->dir_name, count_dirs, 32, 0);
	count = GLOBAL_count;
	printf("%d dirs\n", GLOBAL_count);

	// initialize the mapping of watch descriptors -> dir names
	// TODO: don't require WD_EXTRA, it should manage that auto-
	// magically, dynamically adding more space if needed
	watch_list_init(&self->wds, count + WD_EXTRA);

	// add a watch to every directory
	GLOBAL_ifd = self->ifd;
	GLOBAL_flags = self->iflags;
	GLOBAL_wds = &self->wds;
	nftw(self->dir_name, add_watch, 32, 0);
	GLOBAL_ifd = -1;
	GLOBAL_flags = 0;
	GLOBAL_wds = NULL;

	// XXX: for debugging mostly.
	fflush(stdout);

	// TODO: nftw on every file
	GLOBAL_self = self;
	nftw(self->dir_name, on_startup, 32, 0);
	GLOBAL_self = NULL;

	// check for changes forever
	while (true) {
		// XXX: will read only return full events?
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

	dirwatch_free(self);

	return NULL;
}

static int
watch_list_init(struct watch_list *self, int len)
{
	if (!self)
		exit_msg("watch_list_init null self");
	if (len <= 0)
		exit_msg("watch_list_init invalid len %d", len);

	self->len = len;
	self->slab = xcalloc(len * sizeof(char *));

	return 0;
}

/*
static struct watch_list *watch_list_new(void)
{
	return xcalloc(sizeof(struct watch_list));
}

static void
watch_list_free(struct watch_list *self)
{
	if (!self)
		exit_msg("watch_list_free null self");
	if (self->slab)
		free(self->slab);
	free(self);
}
*/

static char *
watch_list_get(struct watch_list *self, int wd)
{
	if (wd < 0 || wd > self->len)
		exit_msg("invalid wd: %d (len: %u)", wd, self->len);

	return self->slab[wd];
}

static int
watch_list_put(struct watch_list *self, int wd, char *path)
{
	if (self->slab[wd])
		free(self->slab[wd]);
	self->slab[wd] = path;
	return 0;
}

static int
watch_list_remove(struct watch_list *self, int wd)
{
	return watch_list_put(self, wd, NULL);
}
