/*
* rudial.c
*/

/*
# license--start
#
# This file is part of RUSS tools.
# Copyright (C) 2012 John Marshall
#
# RUSS tools is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end
*/

#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "russ.h"

void
print_usage(char *prog_name) {
	if (strcmp(prog_name, "ruhelp") == 0) {
		fprintf(stderr, 
"usage: ruhelp <addr>\n"
"\n"
"Alias for rudial -h ...\n");
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		fprintf(stderr,
"usage: ruinfo <addr>\n"
"\n"
"Alias for rudial -i ...\n");
	} else if (strcmp(prog_name, "ruls") == 0) {
		fprintf(stderr,
"usage: ruls <addr>\n",
"\n"
"Alias for rudial -l ...\n");
	} else if (strcmp(prog_name, "rudial") == 0) {
		fprintf(stderr,
"usage: rudial [<option>] <addr> [<arg> ...]\n"
"\n"
"Dial service at <addr>. A service may support one or more\n"
"operations (e.g., execute, list); execute is the default operation.\n"
"Most other services are for obtaining state information.\n"
"\n"
"A successful dial will effectively connect the stdin, stdout, and\n"
"stderr of the service to that of the rudial. I.e., input to stdin\n"
"of rudial will be forwarded to the service, stdout and stderr of\n"
"the service will be forwarded to the same of rudial.\n"
"\n"
"An exit value of < 0 indicates a failure to connect. Otherwise a 0\n"
"exit value is returned.\n"
"\n"
"Options:\n"
"-h|--help      print this information\n"
"-i|--info      show server-specific information as zero or more\n"
"               name=value lines\n"
"-l|--list      list services provided by the server as paths to be\n"
"               used in the <addr> argument\n"
"-o|--op <op>   request operation <op> from service\n"
"-t|--timeout <seconds>\n"
"               quit the dial if it does not connect within the\n"
"               given time\n");
	}
}

int
main(int argc, char **argv) {
	struct russ_conn	*conn;
	struct russ_forwarding	fwds[3];
	char			*prog_name;
	char			*op, *addr;
	char			*arg;
	char			*attrv[RUSS_MAX_ATTRC];
	int			argi;
	int			attrc;
	int			timeout;

	prog_name = basename(strdup(argv[0]));
	if (argc < 2) {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(-1);
	}

	/* aliases */
	if (strcmp(prog_name, "ruhelp") == 0) {
		op = "help";
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		op = "info";
	} else if (strcmp(prog_name, "ruls") == 0) {
		op = "list";
	} else {
		op = "execute";
		if (strcmp(prog_name, "rudial") != 0) {
			fprintf(stderr, "warning: unknown alias, running as rudial\n");
			prog_name = "rudial";
		}
	}

	/* options */
	timeout = -1;
	argi = 1;
	attrc = 0;
	while (argi < argc) {
		arg = argv[argi++];

		if (strncmp(arg, "-", 1) != 0) {
			argi--;
			break;
		}
		if ((strcmp(arg, "--help") == 0)
			|| (strcmp(arg, "-h") == 0)) {

			print_usage(prog_name);
			exit(0);
		} else if (strcmp(arg, "--id") == 0) {

			op = "id";
		} else if ((strcmp(arg, "--info") == 0)
			|| (strcmp(arg, "-i") == 0)) {

			op = "info";
		} else if ((strcmp(arg, "--list") == 0)
			|| (strcmp(arg, "-l") == 0)) {

			op = "list";
		} else if (((strcmp(arg, "--op") == 0) || (strcmp(arg, "-o") == 0))
			&& (argi < argc)) {
			
			arg = argv[argi++];
			op = arg;
		} else if (((strcmp(arg, "--timeout") == 0) || (strcmp(arg, "-t") == 0))
			&& (argi < argc)) {

			arg = argv[argi++];
			if (sscanf(argv[argi], "%d", &timeout) < 0) {
				fprintf(stderr, "error: bad timeout value\n");
				exit(-1);
			}
		} else if (((strcmp(arg, "--attr") == 0) || (strcmp(arg, "-a") == 0))
			&& (argi < argc)) {
			
			arg = argv[argi++];
			if (attrc >= RUSS_MAX_ATTRC-1) {
				fprintf(stderr, "error: too many attributes\n");
				exit(-1);
			}
			if (strstr(arg, "=") == NULL) {
				fprintf(stderr, "error: bad attribute format\n");
				exit(-1);
			}
			attrv[attrc++] = arg;
			attrv[attrc] = NULL;
		} else {
			fprintf(stderr, "error: bad option and/or missing arguments\n");
			exit(-1);
		}
	}

	/* addr and args */
	if (argi+1 > argc) {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(-1);
	}
	addr = argv[argi++];
	if ((conn = russ_dialv(addr, op, timeout, attrv, argc-argi, &(argv[argi]))) == NULL) {
		fprintf(stderr, "error: cannot dial service\n");
		exit(-1);
	}

	/* start forwarding threads */
	russ_forwarding_init(&(fwds[0]), 0, STDIN_FILENO, conn->fds[0], -1, 16384, 0);
	russ_forwarding_init(&(fwds[1]), 1, conn->fds[1], STDOUT_FILENO, -1, 16384, 0);
	russ_forwarding_init(&(fwds[2]), 1, conn->fds[2], STDERR_FILENO, -1, 16384, 0);
	if (russ_forward_bytes(3, fwds) < 0) {
		fprintf(stderr, "error: could not forward bytes\n");
		exit(-1);
	}

	russ_close_conn(conn);
	conn = russ_free_conn(conn);
	exit(0);
}
