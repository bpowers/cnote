/*===--- net.c - network utility functions ------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 *
 * Licensed under the AGPLv2 license
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#include "net.h"
#include "common.h"

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
			exit_perr ("get_socket: getaddrinfo");

		fprintf(stderr, "get_socket: getaddrinfo: %s\n",
			 gai_strerror (err));
		exit(EXIT_FAILURE);
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sock = socket (rp->ai_family, rp->ai_socktype,
			       rp->ai_protocol);
		if (sock == -1)
			continue;
		if (bind (sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close (sock);
	}

	if (rp == NULL)
		exit_perr ("get_socket: couldn't bind");

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
