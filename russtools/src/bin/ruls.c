/*
* ruls.c
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
		printf(
"usage: ruhelp [-t|--timeout <seconds>] <addr>\n"
"\n"
"Get help for service at <addr>.\n"
);
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		printf(
"usage: ruinfo [-t|--timeout <seconds>] <addr>\n"
"\n"
"Get information about service at <addr>\n"
);
	} else if (strcmp(prog_name, "ruls") == 0) {
		printf(
"usage: ruls [-t|--timeout <seconds>] <addr>\n"
"\n"
"List service(s) at <addr>.\n"
);
	} else {
		fprintf(stderr, "error: unknown program name\n");
	}
}

int
main(int argc, char **argv) {
	struct russ_conn	*conn;
	struct russ_forwarding	fwds[3];
	char			*prog_name;
	char			*addr;
	int			timeout;

	prog_name = basename(strdup(argv[0]));

	/* parse args */
	if (argc == 2) {
		if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
			print_usage(prog_name);
			exit(0);
		}
		timeout = -1;
		addr = argv[1];
	} else if (argc == 5) {
		if ((strcmp(argv[1], "-t") == 0) || (strcmp(argv[1], "--timeout") == 0)) {
			if (sscanf(argv[2], "%d", &timeout) < 0) {
				fprintf(stderr, "error: bad timeout value\n");
				exit(-1);
			}
		} else {
			fprintf(stderr, "error: bad/missing arguments\n");
		}
		addr = argv[3];
	} else {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(-1);
	}

	if (strcmp(prog_name, "ruhelp") == 0) {
		conn = russ_help(addr, timeout);
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		conn = russ_info(addr, timeout);
	} else if (strcmp(prog_name, "ruls") == 0) {
		conn = russ_list(addr, timeout);
	} else {
		fprintf(stderr, "error: unknown program name\n");
		exit(-1);
	}
	if (conn == NULL) {
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