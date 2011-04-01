/*===--- common.h - common functions ------------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _COMMON_H_
#define _COMMON_H_

// for asprintf
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <sqlite3.h>

// the '!!' is from Love's LSP book.  not quite sure why they're
// desirable.  The ones listed on KernelTrap don't have them.
#define likely(x)   __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)
#define __packed    __attribute__((packed))
// prefetch a variable for reading into the L1 cache
#define prefetch(x) __builtin_prefetch(x)
#define __unused__    __attribute__((unused))

typedef void *(*pthread_routine)(void *);

// backlog arg for listen(2); max clients to keep in queue
extern const int BACKLOG;
extern const char *program_name;
extern int verbosity;

struct req;
struct evhttp_request;

struct ops {
	char *(*list)(struct req *self);
	char *(*query)(struct req *self, const char *name);
};

struct req {
	struct ops *ops;
	struct evhttp_request *req;
	sqlite3 *db;
};

#endif // _COMMON_H_
