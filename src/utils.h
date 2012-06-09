// Copyright 2012 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#ifndef _UTILS_H_
#define _UTILS_H_
#include "common.h"

#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>


#define path_join(dst, a, b) {					\
		dst = alloca(strlen(a) + strlen(b) + 2);	\
		strcpy(dst, a);					\
		strcat(dst, "/");				\
		strcat(dst, b);					\
	}

#define strjoin(dst, a, b) {					\
		dst = alloca(strlen(a) + strlen(b) + 2);	\
		strcpy(dst, a);					\
		strcat(dst, b);					\
	}

extern const char *program_name;

static const mode_t S_755 = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
static const mode_t S_644 = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

#define DEBUG (3)
#define INFO  (2)
#define WARN  (1)
#define ERROR (0)

#define log(level, msg_fmt, args...) log_(level, msg_fmt, ## args)
// same as log, but it adds the function name as a prefix to the msg
#define logf(level, msg_fmt, args...) log_(level, "%s: " msg_fmt, __func__, ## args)
void log_(int level, const char *msg_fmt, ...);

// allocates memory or exits
void *xcalloc(size_t size);
void *xmalloc(size_t size);

// calls perror with msg and exits, indicating failure
void exit_perr(const char *msg_fmt, ...);
// prints msg to stderr and exits, indicating failure
void exit_msg(const char *msg_fmt, ...);

// recursive mkdirr.  non-reentrent.
int mkdirr(const char *path, mode_t mode);

int utils_sha256(uint8_t *hash, void *data, size_t len);
const char *sha256_hex_file(const char *path, size_t len);

// passed to evbuffer_add_reference as a function to call when the
// buffer it points to isn't referenced anymore.
void free_cb(const void *data, size_t datalen, void *extra);

#endif // _UTILS_H_
