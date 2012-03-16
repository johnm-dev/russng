/*
** bin/rusrv_echo.c
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

int
req_handler(struct russ_conn *conn) {
	if (strcmp(conn->req.op, "execute") == 0) {
		/* serve the input from fd passed to client */
		char	buf[1024];
		ssize_t	n;

		while ((n = russ_read(conn->fds[0], buf, sizeof(buf))) > 0) {
			russ_writen(conn->fds[1], buf, n);
		}
	} else {
		return -1;
	}
	return 0;
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
	struct russ_listener	*lis;
	char			*saddr;

	signal(SIGCHLD, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(-1);
	}
	saddr = argv[1];

	if ((lis = russ_announce(saddr, 0666, getuid(), getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_loop(lis, req_handler);
}
