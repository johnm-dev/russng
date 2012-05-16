/*
** lib/srv.c
*/

/*
# license--start
#
# This file is part of the RUSS library.
# Copyright (C) 2012 John Marshall
#
# The RUSS library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
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

/**
* Loop to listen for and accept incoming connections.
*
* @param lis	listener object
* @param handler	handler function to call on connection
*/
void
russ_loop(struct russ_listener *lis, russ_req_handler handler) {
	struct russ_conn	*conn;

	while (1) {
		if ((conn = russ_listener_answer(lis, RUSS_TIMEOUT_NEVER)) == NULL) {
			fprintf(stderr, "error: cannot answer connection\n");
			continue;
		}
		if (fork() == 0) {
			russ_listener_close(lis);
			lis = russ_listener_free(lis);
			if ((russ_conn_await_request(conn) < 0)
				|| (russ_conn_accept(conn, NULL, NULL) < 0)) {
				exit(-1);
			}
			exit(handler(conn));
		}
		russ_conn_close(conn);
		conn = russ_conn_free(conn);
	}
}
