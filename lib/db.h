/*===--- tags.c - tag manipulation functions ----------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#include <sqlite3.h>

#define PREPARE_QUERY(db, in, out) do {					\
		err = sqlite3_prepare_v2(db,				\
					 in,				\
					 strlen(in),			\
					 out,				\
					 NULL);				\
		if (err != SQLITE_OK)					\
			exit_msg("sqlite3 prepare '%s' error: %d", in, err); \
	} while (false)
