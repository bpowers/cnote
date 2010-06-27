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

// for libnss3's sha256 hash function
#include <nss/sechash.h>

#include <event2/event.h>


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


void __attribute__ ((format (printf, 1, 2)))
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


void __attribute__ ((format (printf, 1, 2)))
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


int
sha256 (uint8_t *hash, void *data, size_t len)
{
	uint32_t hash_len;
	HASHContext *ctx;

	if (unlikely (!hash))
		exit_msg ("%s: null hash", __func__);

	if (unlikely (!data))
		exit_msg ("%s: null data", __func__);

	ctx = NULL;//HASH_Create (HASH_AlgSHA256);
	if (!ctx)
		exit_perr ("%s: HASH_Create", __func__);
//	HASH_Begin (ctx);
//	HASH_Update (ctx, data, len);
//	HASH_End (ctx, hash, &hash_len, SHA256_LENGTH);
//	HASH_Destroy (ctx);

	return 0;
}
