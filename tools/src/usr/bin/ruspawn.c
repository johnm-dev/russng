/*
* ruspawn.c
*/

/*
# license--start
#
# Copyright 2017 John Marshall
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# license--end
*/

#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <russ/russ.h>

/**
* Do ruspawn.
*
* Does not return.
*
* @param argc		command line argc
* @param argv		command line argv
*/
static void
ruspawn(int argc, char **argv) {
	struct russ_conf	*conf;
	char			*p, *startstr;
	int			withpids = 0;

	/* special handling of --withpids */
	if (strcmp(argv[1], "--withpids") == 0) {
		withpids = 1;
		russ_sarray0_remove(argv, 1);
		argc--;
	}

	/* load conf */
	if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot load configuration.\n");
		exit(1);
	}

	if ((startstr = russ_start(RUSS_STARTTYPE_SPAWN, conf)) == NULL) {
		fprintf(stderr, "error: cannot spawn server\n");
		exit(1);
	}
	if (!withpids) {
		char	*p;

		p = index(startstr, ':');
		p = index(p+1, ':');
		strcpy(startstr, p+1);
	}
	printf("%s", startstr);
	exit(0);
}

/**
* Do rustart.
*
* Does not return.
*
* @param argc		command line argc
* @param argv		command line argv
*/
static void
rustart(int argc, char **argv) {
	struct russ_conf	*conf;

	/* load conf */
	if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot load configuration.\n");
		exit(1);
	}

	signal(SIGPIPE, SIG_IGN);
	russ_start(RUSS_STARTTYPE_START, conf);
	fprintf(stderr, "error: cannot start server\n");
	exit(1);
}

void
ruspawn_print_usage(char *prog_name) {
	printf(
"usage: ruspawn (-c <name>=<value>|-f <path>|--fd <fd>) [...] [-- ...]\n"
"\n"
"Spawn a russ server. Using the configuration, a socket file is\n"
"created and the listener socket is passed to the server. The path\n"
"the socket file is output to stdout.\n"
"\n"
"ruspawn is different from rustart in the following ways. If\n"
"main:addr (the socket file path) is not specified, a path is\n"
"dynamically chosen and used to set main:addr. A reaper process is\n"
"started to automatically cleanup the socket file when the server\n"
"exits. If the server or the repear are signaled, they both will be\n"
"terminated and the socket file cleaned up.\n"
"\n"
"ruspawn is the preferred way to start a server.\n"
"\n"
"Where:\n"
"-c <name>=<value>\n"
"        Set configuration attribute.\n"
"-f <path>\n"
"        Load configuration file.\n"
"--fd <fd>\n"
"        Load configuration from file descriptor.\n"
"-- ...	Arguments to pass to the server program.\n"
);
}

void
rustart_print_usage(char *prog_name) {
	printf(
"usage: rustart (-c <name>=<value>|-f <path>|--fd <fd>) [...] [-- ...]\n"
"\n"
"Start a russ server. Using the configuration, a socket file is\n"
"created and the listener socket is passed to the server.\n"
"\n"
"Where:\n"
"-c <name>=<value>\n"
"        Set configuration attribute.\n"
"-f <path>\n"
"        Load configuration file.\n"
"--fd <fd>\n"
"        Load configuration from file descriptor.\n"
"-- ...	Arguments to pass to the server program.\n"
);
}

int
main(int argc, char **argv) {
	char	*progname;

	progname = basename(strdup(argv[0]));
	if (strcmp(progname, "ruspawn") == 0) {
		if ((argc == 2) && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
			ruspawn_print_usage(progname);
			exit(0);
		}
		ruspawn(argc, argv);
	} else if (strcmp(progname, "rustart") == 0) {
		if ((argc == 2) && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
			rustart_print_usage(progname);
			exit(0);
		}
		rustart(argc, argv);
	} else {
		fprintf(stderr, "error: program name not ruspawn or rustart\n");
		exit(1);
	}
}
