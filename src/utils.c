// Copyright 2012 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#include "utils.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
// for TCP_NODELAY
#include <netinet/tcp.h>
// for O_NONBLOCK
#include <fcntl.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <event2/event.h>
#include <glib.h>

// backlog arg for listen(2); max clients to keep in queue
const int BACKLOG = 256;
int verbosity = WARN;


void *
xcalloc(size_t size)
{
	void *ret;

	// calloc returns 0'ed memory
	ret = calloc(1, size);
	if (unlikely(!ret))
		exit_perr("xcalloc");
	return ret;
}


void *
xmalloc(size_t size)
{
	void *ret;

	// calloc returns 0'ed memory
	ret = malloc(size);
	if (unlikely(!ret))
		exit_perr("xmalloc");
	return ret;
}


void __attribute__ ((format(printf, 2, 3)))
log(int level, const char *msg_fmt, ...)
{
	char *msg;
	va_list args;
	int ret;

	if (likely(level > verbosity))
		return;

	va_start(args, msg_fmt);
	ret = vasprintf(&msg, msg_fmt, args);
	va_end(args);

	if (ret == -1)
		fprintf(stderr, "%s: vasprintf failed.\n", __func__);
	else
		fprintf(stderr, "%s\n", msg);
	free(msg);
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
		fprintf(stderr, "%s: %s.\n", err_msg, strerror(err));

	exit(EXIT_FAILURE);
}


/*----------------------------------------------------------------------
 * Portable function to set a socket into nonblocking mode.
 * Calling this on a socket causes all future read() and write() calls on
 * that socket to do only as much as they can immediately, and return
 * without waiting.
 * If no data can be read or written, they return -1 and set errno
 * to EAGAIN (or EWOULDBLOCK).
 * Thanks to Bjorn Reese for this code.
 *
 * from:
 * http://www.kegel.com/dkftpbench/nonblocking.html (<3 Dan Kegel)
 *--------------------------------------------------------------------*/
int set_nonblocking(int fd)
{
	int flags;

// If they have O_NONBLOCK, use the Posix way to do it
#if defined(O_NONBLOCK)
	// Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and
	// AIX 3.2.5.
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	// Otherwise, use the old way of doing it
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}


int
get_tcp_socket(const char *addr, const char *port)
{
	int sock, err, flag;
	struct addrinfo hints;
	struct addrinfo *result, *rp;

	sock = -1;
	result = NULL;

	memset(&hints, 0, sizeof(hints));

	// FIXME: only support IPv4 for now? AF_UNSPEC would allow
	// both/either
	hints.ai_family = AF_INET; // AF_UNSPEC
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(addr, port, &hints, &result);
	if (err) {
		if (err == EAI_SYSTEM)
			exit_perr ("get_socket: getaddrinfo");

		fprintf(stderr, "get_socket: getaddrinfo: %s\n",
			gai_strerror (err));
		exit(EXIT_FAILURE);
	}

	// bind to the first one we can.
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock = socket(rp->ai_family, rp->ai_socktype,
			      rp->ai_protocol);
		if (sock == -1)
			continue;
		if (bind(sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		close(sock);
	}

	if (rp == NULL || sock == -1)
		exit_perr("get_socket: couldn't bind");

	freeaddrinfo(result);

	// disables the Nagle algorithm, means that all outgoing traffic will
	// be sent immediately, instead of coalesced to cut down on the number
	// of TCP connections
	flag = 1;
	err = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag,
			 sizeof(flag));
	if (err)
		exit_perr("get_socket: disable Nagle");

	err = listen(sock, BACKLOG);
	if (err)
		exit_perr("get_socket: listen");

	err = set_nonblocking(sock);
	if (err)
		exit_perr("get_socket: set_nonblocking");

	return sock;
}


int
event_add_w_timeout(struct event *ev, long int seconds)
{
	struct timeval timeout;
	timeout.tv_sec = seconds;
	timeout.tv_usec = 0;

	return event_add(ev, &timeout);
}


/*
int
utils_sha256(uint8_t *hash, void *data, size_t len)
{
	uint32_t hash_len;
	HASHContext *ctx;

	if (unlikely(!hash))
		exit_msg("sss_sha256: null hash");

	if (unlikely(!data))
		exit_msg("sss_sha256: null data");

	ctx = HASH_Create(HASH_AlgSHA256);
	if (!ctx)
		exit_perr("sss_sha256: HASH_Create");
	HASH_Begin(ctx);
	HASH_Update(ctx, data, len);
	HASH_End(ctx, hash, &hash_len, SHA256_LENGTH);
	HASH_Destroy(ctx);

	return 0;
}
*/

int
mkdirr(const char *path, mode_t mode)
{
	char *dirdup, *dir;
	struct stat info;
	int err;

	dirdup = strdupa(path);
	dir = dirname(dirdup);

	if (access(dir, F_OK) == -1) {
		if (errno != ENOENT)
			return -1;
		// need to recurse with a copy of dir, becuase basename
		// and dirname can modify the string passed in.
		err = mkdirr(dirdup, mode);
		if (err)
			return -1;
	}

	if (stat(path, &info) == -1) {
		if (mkdir(path, mode) == -1) {
			log(ERROR, "%s: mkdir failed for '%s'", __func__,
			    path);
			return -1;
		}
		if (stat(path, &info) == -1) {
			log(ERROR, "%s: stat failed for '%s'", __func__, path);
			return -1;
		}
	}

	if (S_ISDIR(info.st_mode))
		return 0;

	log(ERROR, "%s: path '%s' not a directory", __func__, path);
	return -1;
}


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
