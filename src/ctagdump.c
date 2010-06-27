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
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

static void print_help(void);
static void print_version(void);


static inline void
process_file(const char *file_name)
{
	TagLib_File *file;
	TagLib_Tag *tag;
	const TagLib_AudioProperties *properties;

	file = taglib_file_new(file_name);
	if (file == NULL) {
		fprintf(stderr,
			"%s: WARNING: couldn't open '%s' for reading id3 tags.\n",
			program_name, file_name);
		return;
	}

	

	taglib_tag_free_strings();
	taglib_file_free(file);
}


int
main(int argc, char *const argv[])
{
	int optc;

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
		default:
			exit_msg("%s: unknown option '%c'", program_name, optc);
		}
	}

	if (optind == argc)
		exit_msg("%s: no input file specified", program_name);

	for (int i = optind; i < argc; ++i)
		process_file(argv[i]);

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
