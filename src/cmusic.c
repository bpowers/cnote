/*===--- exaample.c - Example cfunc program ------------------------------===//
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

// clone(2)
#include <sched.h>

#include <event2/event.h>



static const char DEFAULT_PORT[] = "1969";
static const char DEFAULT_ADDR[] = "localhost";

// should be stuck into a config.h eventually
const char CANONICAL_NAME[] = "cmusic";
const char PACKAGE[] = "cmusic";
const char VERSION[] = "0.0.1";
const char YEAR[] = "2010";
const char PACKAGE_BUGREPORT[] = "bobbypowers@gmail.com";

// global var available to various functions that want to report status
const char *program_name;

static const struct option longopts[] =
{
	{"address", required_argument, NULL, 'a'},
	{"port", required_argument, NULL, 'p'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{NULL, 0, NULL, 0}
};

static void print_help(void);
static void print_version(void);


void *
cfunc_car(struct cfunc_cons *coll)
{
	return coll->ops->car(coll);
}

struct cfunc_cons *
cfunc_cdr(struct cfunc_cons *coll)
{
	return coll->ops->cdr(coll);
}

struct cfunc_cons *
cfunc_map(cfunc_closure_t f, struct cfunc_cons *coll)
{
	void *a = cfunc_car(coll);
	struct cfunc_cons *c = coll;
	for (; a != NULL; a = c->car, c=c->cdr)
	{
		f(a);
	}
	return 0;
}


void
on_accept (int fd, short event, struct ccgi_state *state)
{
	int conn;
	//pid_t pid;

	if (unlikely (!state))
		exit_msg ("%s: null state", __func__);

	conn = accept(state->socket, NULL, NULL);
	if (unlikely(conn < 1)) {
		fprintf (stderr, "%s: accept failed!\n", __func__);
		return;
	}

	//clone(new_request, );
}


int main(int argc, char *const argv[])
{
	int optc, err;
	const char *addr, *port;
	struct ccgi_state state;
	struct sigaction sa = {.sa_handler = SIG_IGN};

	/* Make stderr line buffered - we only use it for debug info */
	setvbuf(stderr, NULL, _IOLBF, 0);

	program_name = argv[0];
	addr = DEFAULT_ADDR;
	port = DEFAULT_PORT;

	while ((optc = getopt_long(argc, argv,
				   "a:p:hv", longopts, NULL)) != -1) {
		switch (optc) {
		// GNU standards have --help and --version exit immediately.
		case 'v':
			print_version();
			return 0;
		case 'h':
			print_help();
			return 0;
		case 'a':
			addr = (const char *)optarg;
			break;
		case 'p':
			port = (const char *)optarg;
			break;
		default:
			fprintf(stderr, "unknown option '%c'", optc);
			exit(EXIT_FAILURE);
		}
	}

	// automatically reap zombies
	sigaction(SIGCHLD, &sa, NULL);

	state.socket = get_listen_socket(addr, port);
	state.ev_base = event_base_new();
	if (!state.ev_base)
		exit_perr("main: event_base_new");

	state.ev_accept = event_new(state.ev_base, state.socket,
				    EV_READ|EV_PERSIST,
				    (event_callback_fn)on_accept, &state);
	if (!state.ev_accept)
		exit_perr("main: event_set");

	err = event_add(state.ev_accept, NULL);
	if (err)
		exit_perr("main: event_add");

	fprintf(stderr, "%s: initialized and waiting for connections\n",
		program_name);

	// the main event loop
	event_base_dispatch(state.ev_base);

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
	fputs ("\
  -a, --address=ADDR  address to listen for connections on\n\
                      (default: localhost)\n", stdout);
	fputs ("\
  -p, --port=PORT     port to listen for connections on\n\
                      (default: 1984)\n", stdout);
	printf ("\n");
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
