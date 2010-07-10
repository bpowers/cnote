/*===--- ctagdump.c - dump id3 tags -------------------------------------===//
 *
 * Copyright 2010 Bobby Powers
 *
 * This file is part of cfunc.
 *
 * cfunc is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 *
 * cfunc is distributed in the hope that it will be useful, but WITHOUT
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cfunc.  If not, see <http://www.gnu.org/licenses/>.
 *
 *===--------------------------------------------------------------------===//
 *
 * Just an example of how to use cfunc.
 *
 *===--------------------------------------------------------------------===//
 */
#include <cfunc/cfunc.h>
#include <cfunc/common.h>
#include <cfunc/net.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <signal.h>
#include <unistd.h>
#include <getopt.h>

#include <time.h>
#include <sys/stat.h>

#include <taglib/tag_c.h>

#include <postgresql/libpq-fe.h>


static const char DEFAULT_PORT[] = "1969";
static const char DEFAULT_ADDR[] = "localhost";

// should be stuck into a config.h eventually
const char CANONICAL_NAME[] = "ctagdump";
const char PACKAGE[] = "ctagdump";
const char VERSION[] = "0.0.1";
const char YEAR[] = "2010";
const char PACKAGE_BUGREPORT[] = "bobbypowers@gmail.com";

// global var available to various functions that want to report status
const char *program_name;

static const struct option longopts[] =
{
	{"base-path", required_argument, NULL, 'b'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

static void print_help(void);
static void print_version(void);


static inline PGresult *
pg_exec(PGconn *conn, const char *query)
{
	PGresult *res;

	res = PQexec(conn, query);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "'%s' command failed: %s", query,
			PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		exit_msg("");
	}
	return res;
}


static inline void
process_file(const char *file_path, const char *base_path, PGconn *conn)
{
	TagLib_File *file;
	TagLib_Tag *tag;
	PGresult *res;
	const TagLib_AudioProperties *props;
	const char *rel_path;
	char mtime[26];
	struct stat stats;
	size_t len;
	const char *query_args[8];
	char *query_fmt = 
		"INSERT INTO music (title, artist, album, track, time, path, hash, modified)" 
		"    VALUES ($1, $2, $3, $4, $5, $6, $7, $8)";

	file = taglib_file_new(file_path);
	if (file == NULL) {
		fprintf(stderr,
			"%s: WARNING: couldn't open '%s' for reading id3 tags.\n",
			program_name, file_path);
		return;
	}
	tag = taglib_file_tag(file);
	props = taglib_file_audioproperties(file);
	if (tag == NULL || props == NULL) {
		fprintf(stderr,
			"%s: WARNING: couldn't open '%s's tags or props.\n",
			program_name, file_path);
		return;
	}

	rel_path = file_path;
	if (base_path) {
		len = strlen(base_path);
		if (!strncmp(file_path, base_path, len))
			rel_path = &file_path[len];
	}

	stat(file_path, &stats);
	ctime_r(&stats.st_mtime, mtime);
	// remove the trailing newline
	mtime[strlen(mtime)-1] = '\0';

	query_args[0] = taglib_tag_title(tag);
	query_args[1] = taglib_tag_artist(tag);
	query_args[2] = taglib_tag_album(tag);
	asprintf((char **)&query_args[3], "%d", taglib_tag_track(tag));
	asprintf((char **)&query_args[4], "%d",
		 taglib_audioproperties_length(props));
	query_args[5] = rel_path;
	query_args[6] = sha256_hex_file(file_path, stats.st_size);
	query_args[7] = mtime;

	res = PQexecParams(conn, query_fmt, 8, NULL,
			   query_args, NULL, NULL, 0);
	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		fprintf(stderr, "'%s' command failed: %s", query_fmt,
			PQerrorMessage(conn));
		PQclear(res);
		PQfinish(conn);
		exit_msg("");
	}

	// free the generated sha256 hex string
	free((void *)query_args[6]);

	taglib_tag_free_strings();
	taglib_file_free(file);
}


int
main(int argc, char *const argv[])
{
	int optc;
	PGconn *conn;
	const char *base_path = NULL;
	const char *conn_info = "dbname = cmusic";

	/* Make stderr line buffered - we only use it for debug info */
	setvbuf(stderr, NULL, _IOLBF, 0);

	program_name = argv[0];

	while ((optc = getopt_long(argc, argv,
				   "hv", longopts, NULL)) != -1) {
		switch (optc) {
		// GNU standards have --help and --version exit immediately.
		case 'v':
			print_version();
			return 0;
		case 'h':
			print_help();
			return 0;
		case 'b':
			base_path = optarg;
			break;
		default:
			exit_msg("%s: unknown option '%c'", program_name, optc);
		}
	}

	if (optind == argc)
		exit_msg("%s: no input file specified", program_name);

	conn = PQconnectdb(conn_info);
	if (PQstatus(conn) != CONNECTION_OK) {
		PQfinish(conn);
		exit_msg("%s: couldn't connect to postgres", program_name);
	}

	pg_exec(conn, "BEGIN");

	for (int i = optind; i < argc; ++i)
		process_file(argv[i], base_path, conn);

	pg_exec(conn, "END");

	PQfinish(conn);
	return 0;
}


static void
print_help()
{
	printf ("\
Usage: %s [-aphv]\n", program_name);
	fputs ("\
SCGI RESTful access to your music collection.\n\n\
Options:\n", stdout);
	puts ("");
	fputs ("\
  -h, --help          display this help and exit\n\
  -v, --version       display version information and exit\n", stdout);
	puts ("");
	printf ("\
Report bugs to <%s>.\n", PACKAGE_BUGREPORT);
}


static void
print_version (void)
{
	printf ("%s (%s) %s\n", CANONICAL_NAME, PACKAGE, VERSION);
	puts ("");
	// FSF recommends seperating out the year, for ease in translations.
	printf ("\
Copyright (C) %s Bobby Powers.\n\
License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n", YEAR);
}
