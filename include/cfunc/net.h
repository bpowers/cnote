/*===--- net.h - net functions ------------------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _NET_H_
#define _NET_H_

#define _GNU_SOURCE
#include <string.h>

// returns an open TCP socket 
int get_listen_socket(const char *addr, const char *port);
int set_nonblocking (int fd);
int set_blocking (int fd);

size_t ns_reads(int fd, char **data);
char **read_env(int fd);

#endif // _NET_H_
