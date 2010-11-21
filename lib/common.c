/*===--- common.c - common functions ----------------------------===//
 *
 * Copyright 2010 Bobby Powers
 *
 * Licensed under the GPLv2 license
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#include "cfunc/common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
// for TCP_NODELAY
#include <netinet/tcp.h>
// for O_NONBLOCK
#include <fcntl.h>

#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <openssl/sha.h>

#include <event2/event.h>
#include <glib.h>


// backlog arg for listen(2); max clients to keep in queue
const int BACKLOG = 256;

void *
xmalloc0 (size_t size)
{
	void *ret;

	// calloc returns 0'ed memory
	ret = calloc(1, size);
	if (unlikely(!ret))
		exit_perr("xmalloc0");
	return ret;
}


void *
xmalloc (size_t size)
{
	void *ret;

	ret = malloc(size);
	if (unlikely(!ret))
		exit_perr("xmalloc");
	return ret;
}


void __attribute__ ((noreturn,format(printf, 1, 2)))
exit_msg(const char *err_fmt, ...)
{
	va_list args;
	char *err_msg;
	int ret, err;

	// record errno, because it could change in the call to vasprintf
	err = errno;

	va_start(args, err_fmt);
	ret = vasprintf(&err_msg, err_fmt, args);
	va_end(args);

	if (ret == -1)
		fprintf(stderr, "%s: vasprintf failed.\n", __func__);
	else
		fprintf(stderr, "%s\n", err_msg);

	exit(EXIT_FAILURE);
}


void __attribute__ ((noreturn,format(printf, 1, 2)))
exit_perr(const char *err_fmt, ...)
{
	va_list args;
	char *err_msg;
	int ret, err;

	// record errno, because it could change in the call to vasprintf
	err = errno;

	va_start(args, err_fmt);
	ret = vasprintf(&err_msg, err_fmt, args);
	va_end(args);

	if (ret == -1)
		fprintf(stderr, "%s: vasprintf failed.\n", __func__);
	else
		fprintf(stderr, "%s: %s.\n", err_msg, strerror (err));

	exit(EXIT_FAILURE);
}


/*
int
sha256 (uint8_t *hash, void *data, size_t len)
{
	if (unlikely (!hash))
		exit_msg ("%s: null hash", __func__);

	if (unlikely (!data))
		exit_msg ("%s: null data", __func__);

	SHA256 (data, len, hash);

	return 0;
}
*/


const char *
sha256_hex_file(const char *path __unused__, size_t len __unused__)
{
	return "0000000000000000000000000000000000000000";
/*
	int fd;
	uint8_t sha256[SHA256_DIGEST_LENGTH];
	char *ret;
	void *fmap;

	if (unlikely(!path))
		exit_msg("%s: called with null path", __func__);
	if (unlikely(len <= 0))
		exit_msg("%s: called with 0 or negative len", __func__);

	// malloc a string to return the hex version of the sha256 hash
	// the +1 is for the trailing null
	ret = xmalloc(2 * SHA256_DIGEST_LENGTH + 1);

	fd = open(path, O_NOATIME|O_RDONLY);
	if (fd < 0)
		exit_perr("%s: open failed on '%s'", __func__, path);

        fmap = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	if (!fmap)
		exit_perr("%s: mmap failed", __func__);

	SHA256(fmap, len, sha256);
	for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
		snprintf(&ret[2 * i], 3, "%02x", sha256[i]);

	munmap(fmap, len);
	close(fd);

	return ret;
*/
}


void
free_cb(const void *data, size_t datalen __unused__, void *extra __unused__)
{
	g_free((gpointer)data);
}


struct watch_list *watch_list_new(void)
{
	return xmalloc0(sizeof(struct watch_list));
}

int
watch_list_init(struct watch_list *self, int len)
{
	if (!self)
		exit_msg("watch_list_init null self");
	if (len <= 0)
		exit_msg("watch_list_init invalid len %d", len);

	self->len = len;
	self->slab = xmalloc0(len * sizeof(char *));

	return 0;
}

void
watch_list_free(struct watch_list *self)
{
	if (!self)
		exit_msg("watch_list_free null self");
	if (self->slab)
		free(self->slab);
	free(self);
}

char *
watch_list_get(struct watch_list *self, int wd)
{
	if (wd < 0 || wd > self->len)
		exit_msg("invalid wd: %d (len: %u)", wd, self->len);

	return self->slab[wd];
}

int
watch_list_put(struct watch_list *self, int wd, char *path)
{
	if (self->slab[wd])
		free(self->slab[wd]);
	self->slab[wd] = path;
	return 0;
}

int
watch_list_remove(struct watch_list *self, int wd)
{
	return watch_list_put(self, wd, NULL);
}
