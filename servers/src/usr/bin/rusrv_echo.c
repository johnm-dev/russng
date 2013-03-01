/*
** bin/rusrv_echo.c
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

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "russ.h"

void
req_handler(struct russ_conn *conn) {
	if (conn->req.op == RUSS_OP_EXECUTE) {
		/* serve the input from fd passed to client */
		char	buf[1024];
		ssize_t	n;

		while ((n = russ_read(conn->fds[0], buf, sizeof(buf))) > 0) {
			russ_writen(conn->fds[1], buf, n);
		}
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_echo <saddr>\n"
"\n"
"Russ-based echo server.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_lis	*lis;
	char		*saddr;

	signal(SIGCHLD, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(1);
	}
	saddr = argv[1];

	if ((lis = russ_announce(saddr, 0666, getuid(), getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_lis_loop(lis, NULL, NULL, req_handler);
}
