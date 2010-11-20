/*===--- db.h - database abstraction ------------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 * Licensed under the GPLv2 license
 *
 * see COPYING for details.
 *
 *===--------------------------------------------------------------------===//
 */
#ifndef _DB_H_
#define _DB_H_

#include <libpq-fe.h>

PGresult *pg_exec(PGconn *conn, const char *query);

#endif // _DB_H_
