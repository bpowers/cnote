/*===--- tags.h - id3 tag manipulation functions ------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _TAGS_H_
#define _TAGS_H_

#include <libpq-fe.h>

void process_file(const char *file_path, const char *base_path, PGconn *conn);

#endif // _TAGS_H_
