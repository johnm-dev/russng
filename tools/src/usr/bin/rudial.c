/*
* rudial.c
*/

/*
# license--start
#
# Copyright 2012 John Marshall
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

#include <libgen.h>
#include <pthread.h>
#include <signal.h>
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
	} else if (strcmp(prog_name, "ruhelp") == 0) {
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
	struct russ_fwd		fwds[RUSS_CONN_NFDS];
	russ_deadline		deadline;
	int			debug;
	int			timeout;
	char			*prog_name;
	char			*op_str, *addr;
	russ_op			op, op_ext;
	char			*arg;
	char			*attrv[RUSS_REQ_ATTRS_MAX];
	int			argi;
	int			attrc;
	int			req_opt_mask;
	int			exit_status;

	signal(SIGPIPE, SIG_IGN);

	prog_name = basename(strdup(argv[0]));

	/* initialize */
	debug = 0;
	deadline = RUSS_DEADLINE_NEVER;
	argi = 1;
	attrc = 0;
	attrv[0] = NULL;
	op = RUSS_OP_NULL;

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
			if (attrc >= RUSS_REQ_ATTRS_MAX-1) {
				fprintf(stderr, "error: too many attributes\n");
				exit(1);
			}
			if (strstr(arg, "=") == NULL) {
				fprintf(stderr, "error: bad attribute format\n");
				exit(1);
			}
			attrv[attrc++] = arg;
			attrv[attrc] = NULL;
		} else if (strcmp(arg, "--debug") == 0) {
			debug = 1;
		} else if ((strcmp(arg, "-h") == 0) || (strcmp(arg, "--help") == 0)) {
			print_usage(prog_name);
			exit(0);
		} else if (((strcmp(arg, "--timeout") == 0) || (strcmp(arg, "-t") == 0))
			&& (argi < argc)) {

			arg = argv[argi++];
			if (sscanf(arg, "%d", (int *)&timeout) < 0) {
				fprintf(stderr, "error: bad timeout value\n");
				exit(1);
			}
			timeout *= 1000;
			deadline = russ_to_deadline(timeout);
		} else {
			fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
			exit(1);
		}
	}

	/* [op], addr and args */
	if ((strcmp(prog_name, "rudial") == 0) || (strcmp(prog_name, "ruexec") == 0)) {
		if ((strcmp(prog_name, "rudial") == 0) 
			&& (argi+2 <= argc)) {
			if (russ_op_lookup(argv[argi++], &op, &op_ext) < 0) {
				fprintf(stderr, "%s\n", RUSS_MSG_BAD_OP);
				exit(1);
			}
			addr = argv[argi++];
			conn = russ_dialv(deadline, op, addr, attrv, &(argv[argi]));
			conn->req->op_ext = op_ext;
		} else if ((strcmp(prog_name, "ruexec") == 0)
			&& (argi+1 <= argc)) {
			addr = argv[argi++];
			conn = russ_execv(deadline, addr, attrv, &(argv[argi]));
		} else {
			fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
			exit(1);
		}
	} else if (strcmp(prog_name, "ruhelp") == 0) {
		if (argi < argc) {
			addr = argv[argi];
			conn = russ_help(deadline, addr);
		} else {
			fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
			exit(1);
		}
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		if (argi < argc) {
			addr = argv[argi];
			conn = russ_info(deadline, addr);
		} else {
			fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
			exit(1);
		}
	} else {
		fprintf(stderr, "error: unknown program name\n");
		exit(1);
	}

	if (conn == NULL) {
		fprintf(stderr, "%s\n", RUSS_MSG_NO_DIAL);
		exit(RUSS_EXIT_CALL_FAILURE);
	}

//fprintf(stderr, "STDIN OUT ERR (%d,%d,%d) fds (%d,%d,%d)\n", STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO, conn->fds[0], conn->fds[1], conn->fds[2]);
	/*
	* initialize forwarders (handing off fds; but not closing)
	* and start threads; STDERR_FILENO is not closed if
	* debugging
	*/
	russ_fwd_init(&(fwds[0]), 0, STDIN_FILENO, conn->fds[0], -1, 65536, 0, RUSS_FWD_CLOSE_INOUT);
	russ_fwd_init(&(fwds[1]), 0, conn->fds[1], STDOUT_FILENO, -1, 65536, 0, RUSS_FWD_CLOSE_INOUT);
	russ_fwd_init(&(fwds[2]), 0, conn->fds[2], STDERR_FILENO, -1, 65536, 0,
		(debug ? RUSS_FWD_CLOSE_IN : RUSS_FWD_CLOSE_INOUT));
	conn->fds[0] = -1;
	conn->fds[1] = -1;
	conn->fds[2] = -1;
	if (russ_fwds_run(fwds, RUSS_CONN_STD_NFDS-1) < 0) {
		fprintf(stderr, "error: could not forward bytes\n");
		exit(1);
	}

	/* wait for exit */
	if (debug) {
		fprintf(stderr, "debug: waiting for connection exit\n");
	}
	if (russ_conn_wait(conn, &exit_status, -1) < 0) {
		fprintf(stderr, "%s\n", RUSS_MSG_BAD_CONN_EVENT);
		exit_status = RUSS_EXIT_SYS_FAILURE;
	}
	if (debug) {
		fprintf(stderr, "debug: exit_status (%d)\n", exit_status);
	}

	russ_fwd_join(&(fwds[1]));
	if (debug) {
		fprintf(stderr, "debug: stdout forwarder joined\n");
	}
	russ_fwd_join(&(fwds[2]));
	if (debug) {
		fprintf(stderr, "debug: stderr forwarder joined\n");
	}

	russ_conn_close(conn);
	conn = russ_conn_free(conn);
	exit(exit_status);
}
