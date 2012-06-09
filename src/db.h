// Copyright 2012 Bobby Powers. All rights reserved.
// Use of this source code is governed by the MIT
// license that can be found in the LICENSE file.
#include <sqlite3.h>

// FIXME: this is gross
#define PREPARE_QUERY(db, in, out) do {					\
		err = sqlite3_prepare_v2(db,				\
					 in,				\
					 strlen(in),			\
					 out,				\
					 NULL);				\
		if (err != SQLITE_OK)					\
			exit_msg("sqlite3 prepare '%s' error: %d", in, err); \
	} while (false)
