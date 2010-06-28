/*===--- net.c - network utility functions ------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 *
 * Licensed under the AGPLv2 license
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#include "cfunc/net.h"
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



// from DJB's short paper on the subject.
#define MAX_SIZE_LEN (9)


static inline size_t
ns_read_size(int fd)
{
	ssize_t err, i;
	char c;
	// leave room for a trailing null pointer
	char size[MAX_SIZE_LEN+1];

	memset(&size, 0, sizeof(size));

	// FIXME: this is inefficient, because we're doing a syscall
	// every iteration, but it gets a lot messier if we use
	// buffered io, so I guess its fine for now.
	i = 0;
	do {
		err = read(fd, &c, 1);
		if (c == ':') {
			return atol(size);
		} else if (unlikely(!err)) {
			exit_msg("%s: read EOF", __func__);
		} else if (err < 0) {
			if (errno == EINTR)
				continue;
			else
				exit_perr("%s: read", __func__);
		}
		size[i++] = c;
	} while (i < MAX_SIZE_LEN);

	return atol(size);
}


size_t
ns_reads(int fd, char **data)
{
	size_t size;
	ssize_t len;
	int offset;
	char c;

	offset = 0;
	c = 0;

	size = ns_read_size(fd);
	if (unlikely(size <= 0))
		exit_msg("%s: invalid size (%d)", __func__, size);

	*data = xmalloc(size);

	while (size > 0) {
		len = read(fd, &(*data)[offset], size);
		if (len == 0)
			exit_perr("%s: short netstring read", __func__);
		offset += len;
		size -= len;
	}

	read(fd, &c, 1);
	if (c != ',')
		exit_msg("%s: missing netstring terminator", __func__);

	return len;
}


// FIXME: not unicode safe I guess.
static int
count_char(const char *str, int len, char c)
{
	int count;
	if (unlikely (!str))
		exit_msg("%s: null str", __func__);

	for (int i = 0; i < len; ++i)
		if (str[i] == c)
			++count;

	return count;
}


GHashTable *
read_env(int fd)
{
	int num_lines, len;
	char *headers;
	char *header;
	GHashTable *ht;

	len = ns_reads(fd, &headers);
	num_lines = count_char(headers, len, '\0');

	if (num_lines % 2)
		exit_msg("%s: odd number of headers; something broke");

	ht = g_hash_table_new(g_str_hash, g_str_equal);
	header = headers;
	for (int i = 0; i < num_lines; ++i) {
		char *key, *value;
		key = header;
		header = &header[strlen(header)+1];
		value = header;
		header = &header[strlen(header)+1];
		if (*key)
			g_hash_table_insert(ht, key, value);
	}

	return ht;
}


int
get_listen_socket(const char *addr, const char *port)
{
	int sock, err, flag;
	struct addrinfo hints;
	struct addrinfo *result, *rp;

	result = NULL;

	memset(&hints, 0, sizeof (hints));
	// FIXME: only support IPv4 for now?
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	err = getaddrinfo(addr, port, &hints, &result);
	if (err) {
		if (err == EAI_SYSTEM)
			exit_perr ("%s: getaddrinfo", __func__);

		fprintf(stderr, "%s: getaddrinfo: %s\n",
			__func__, gai_strerror (err));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		//int on;
		sock = socket (rp->ai_family, rp->ai_socktype,
			       rp->ai_protocol);

		if (sock == -1)
			continue;

		//on = 1;
		//setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (bind (sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close (sock);
	}

	if (rp == NULL)
		exit_perr ("%s: couldn't bind", __func__);

	freeaddrinfo (result);

	// disables the Nagle algorithm, means that all outgoing traffic will
	// be sent immediately, instead of coalesced to cut down on the number
	// of TCP connections
	flag = 1;
	err = setsockopt (sock, IPPROTO_TCP, TCP_NODELAY, &flag,
			  sizeof (flag));
	if (err)
		exit_perr ("get_socket: disable Nagle");

	err = listen (sock, BACKLOG);
	if (err)
		exit_perr ("get_socket: listen");

	err = set_nonblocking (sock);
	if (err)
		perror ("get_socket: set_nonblocking");

	return sock;
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

int set_blocking(int fd)
{
	int flags;

// If they have O_NONBLOCK, use the Posix way to do it
#if defined(O_NONBLOCK)
	// Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and
	// AIX 3.2.5.
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
#else
	// Otherwise, use the old way of doing it
	flags = 0;
	return ioctl(fd, FIOBIO, &flags);
#endif
}


size_t xwrite(int fd, const char *msg, size_t len)
{
	int offset, size;

	if (unlikely (!msg))
		exit_msg("%s: null msg", __func__);

	offset = 0;
	size = len;
	while (size > 0) {
		len = write(fd, &msg[offset], size);
		if (len < 0)
			exit_perr("%s: write failed", __func__);
		offset += len;
		size -= len;
	}

	return offset;
}


size_t __attribute__ ((format (printf, 2, 3)))
vxwrite(int fd, const char *msg_fmt, ...)
{
	va_list args;
	char *msg;
	int ret, err;

	// record errno, because it could change in the call to vasprintf
	err = errno;

	va_start(args, msg_fmt);
	ret = vasprintf(&msg, msg_fmt, args);
	va_end(args);

	if (ret == -1)
		exit_perr("%s: vasprintf failed", __func__);

	ret = xwrite(fd, msg, strlen(msg));

	free(msg);
	return ret;
}
