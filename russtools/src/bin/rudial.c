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
	if (strcmp(prog_name, "rudial") == 0) {
		printf(
"usage: rudial [<option>] <op> <addr> [<arg> ...]\n"
"\n"
"Dial service at <addr> to perform <op>. A service may support one\n"
"or more operations (e.g., execute, help, info, list).\n"
"\n"
"A successful dial will effectively connect the stdin, stdout, and\n"
"stderr of the service. Once connected, rudial forwards the stdin,\n"
"stdout, and sterr I/O data between the caller and the service.\n"
"\n"
"An exit value of < 0 indicates a failure to connect. Otherwise a 0\n"
"exit value is returned.\n"
);
	} else if (strcmp(prog_name, "ruexec") == 0) {
		printf( 
"usage: ruexec [<option>] <addr>\n"
"\n"
"Execute service at <addr>.\n"
);
	} else {
		return;
	}
	/* common help */
	printf(
"\n"
"Options:\n"
"-a|--attr <name=vaue>\n" \
"               pass a 'name=value' string to the service.\n"
"\n"
"-t|--timeout <seconds>\n" \
"               allow a given amount of time to connect before\n"
);
}

int
main(int argc, char **argv) {
	struct russ_conn	*conn;
	struct russ_forwarder	fwds[RUSS_CONN_NFDS];
	russ_timeout		timeout;
	char			*prog_name;
	char			*op, *addr;
	char			*arg;
	char			*attrv[RUSS_MAX_ATTRC];
	int			argi;
	int			attrc;
	int			req_opt_mask;
	int			exit_status;

	prog_name = basename(strdup(argv[0]));

	/* initialize */
	timeout = -1;
	argi = 1;
	attrc = 0;
	attrv[0] = NULL;
	op = NULL;

	/* options */
	while (argi < argc) {
		arg = argv[argi++];

		if (strncmp(arg, "-", 1) != 0) {
			argi--;
			break;
		}

		if (((strcmp(arg, "--attr") == 0) || (strcmp(arg, "-a") == 0))
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
		} else if ((strcmp(arg, "-h") == 0) || (strcmp(arg, "--help") == 0)) {
			print_usage(prog_name);
			exit(0);
		} else if (((strcmp(arg, "--timeout") == 0) || (strcmp(arg, "-t") == 0))
			&& (argi < argc)) {

			arg = argv[argi++];
			if (sscanf(argv[argi], "%d", (int *)&timeout) < 0) {
				fprintf(stderr, "error: bad timeout value\n");
				exit(-1);
			}
			timeout *= 1000;
		} else {
			fprintf(stderr, "error: bad option and/or missing arguments\n");
			exit(-1);
		}
	}

	/* [op], addr and args */
	if ((strcmp(prog_name, "ruexec") == 0)
		&& (argi+1 <= argc)) {
		addr = argv[argi++];
		conn = russ_execv(timeout, addr, attrv, &(argv[argi]));
	} else if ((strcmp(prog_name, "rudial") == 0) 
		&& (argi+2 <= argc)) {
		op = argv[argi++];
		addr = argv[argi++];
		conn = russ_dialv(timeout, op, addr, attrv, &(argv[argi]));
	} else {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(-1);
	}

	if (conn == NULL) {
		fprintf(stderr, "error: cannot dial service\n");
		exit(-1);
	}

	russ_forwarder_init(&(fwds[0]), STDIN_FILENO, conn->fds[0], -1, 16384, 0);
	russ_forwarder_init(&(fwds[1]), conn->fds[1], STDOUT_FILENO, -1, 16384, 0);
	russ_forwarder_init(&(fwds[2]), conn->fds[2], STDERR_FILENO, -1, 16384, 0);
	if (russ_run_forwarders(RUSS_CONN_NFDS, fwds) < 0) {
		fprintf(stderr, "error: could not forward bytes\n");
		exit(-1);
	}
	russ_conn_wait(conn, &exit_status, NULL, -1);
	russ_forwarder_join(&(fwds[1]));

	russ_conn_close(conn);
	conn = russ_conn_free(conn);
	exit(0);
}
