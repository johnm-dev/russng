/*
** bin/rusrv_pass.c
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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#include "russ_conf.h"
#include "russ.h"

/* global */
struct russ_conf	*conf = NULL;

char	*HELP = 
"Provides access to remote user@host using ssh.\n"
"\n"
"... <args>\n"
"    Connect to service ... and pass fds back.\n";

/*
* Example alternate accept handler.
*
* @param self		connection object
* @param nfds		number of fds in cfds array
* @param cfds		satisfies call requirement; ignored
* @param sfds		satisfies call requirement; ignored
* @return		0 on success; -1 on failure
*/
int
alt_russ_conn_accept(struct russ_conn *self, int nfds, int *cfds, int *sfds) {
	struct russ_conn	*conn;
	struct russ_request	*req;

	req = &(self->req);
	if (strcmp(req->spath, "/") == 0) {
		return russ_conn_accept(self, nfds, cfds, sfds);
	}

	if ((conn = russ_dialv(-1, req->op, req->spath, req->attrv, req->argv)) == NULL) {
		return -1;
	}
	return russ_conn_splice(self, conn);
}

/**
* Service request only if it is for "/".
*/
void
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;

	req = &(conn->req);
	if (strcmp(req->spath, "/") == 0) {
		if (strcmp(req->op, "help") == 0) {
			russ_dprintf(conn->fds[1], "%s", HELP);
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else if (strcmp(req->op, "list") == 0) {
			russ_dprintf(conn->fds[1], "%s", HELP);
			russ_conn_fatal(conn, RUSS_MSG_UNDEF_SERVICE, RUSS_EXIT_FAILURE);
		} else {
			/* TODO: something else needs to be here */
			russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
		}
	}
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_pass [<conf options>]\n"
"\n"
"A sample russ-based server to dial a service and pass the fds of\n"
"the dialed service back to the original client. This allows for\n"
"special kinds of services that do something and then get out of\n"
"the way of the client and the dialed service (e.g., scheduler,\n"
"redirector, rewriter).\n"
);
}

void
alt_russ_listener_loop(struct russ_listener *self, russ_req_handler handler) {
	struct russ_conn	*conn;

	while (1) {
		if ((conn = russ_listener_answer(self, RUSS_TIMEOUT_NEVER)) == NULL) {
			fprintf(stderr, "error: cannot answer connection\n");
			continue;
		}

		if (fork() == 0) {
			russ_listener_close(self);
			self = russ_listener_free(self);
			if ((russ_conn_await_request(conn) < 0)
				|| (alt_russ_conn_accept(conn, 0, NULL, NULL) < 0)) {
				exit(-1);
			}
			handler(conn);
			russ_conn_fatal(conn, RUSS_MSG_NO_EXIT, RUSS_EXIT_SYS_FAILURE);
			exit(0);
		}
		russ_conn_close(conn);
		conn = russ_conn_free(conn);
	}
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(-1);
	}

	lis = russ_announce(russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid()));
	if (lis == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	alt_russ_listener_loop(lis, master_handler);
	exit(0);
}
