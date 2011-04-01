/*===--- common functions -----------------------------------------------===//
 *
 * Copyright 2011 Bobby Powers
 * Licensed under the GPLv2 or GPLv3, see COPYING for details.
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


