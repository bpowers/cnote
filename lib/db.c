#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

// wraps simple postgres queries from static strings
PGresult *
pg_exec(PGconn *conn, const char *query)
{
	PGresult *res;
	ExecStatusType status;

	res = PQexec(conn, query);
	status = PQresultStatus(res);
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK)
	{
		fprintf(stderr, "'%s' command failed (%d): %s", query,
			status, PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		exit(EXIT_FAILURE);
	}

	return res;
}
