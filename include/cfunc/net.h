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


// returns an open TCP socket 
int get_listen_socket(const char *addr, const char *port);
int set_nonblocking (int fd);

char *ns_reads(int fd);

#endif // _NET_H_
