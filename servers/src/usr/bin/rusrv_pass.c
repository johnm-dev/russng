/*
** bin/rusrv_pass.c
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
* Example alternate answer handler.
*
* @param self		connection object
* @return		0 on success; -1 on failure
*/
int
alt_answer_handler(struct russ_conn *self) {
	struct russ_conn	*conn;
	struct russ_req		*req;

	req = &(self->req);
	if (strcmp(req->spath, "/") == 0) {
		return russ_standard_answer_handler(self);
	}

	if ((conn = russ_dialv(RUSS_DEADLINE_NEVER, req->op, req->spath, req->attrv, req->argv)) == NULL) {
		return -1;
	}
	return russ_conn_splice(self, conn);
}

/**
* Service request only if it is for "/".
*/
void
master_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req;

	req = &(conn->req);
	if (strcmp(req->spath, "/") == 0) {
		switch (req->opnum) {
		case RUSS_OPNUM_HELP:
			russ_dprintf(conn->fds[1], "%s", HELP);
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			break;
		case RUSS_OPNUM_LIST:
			russ_dprintf(conn->fds[1], "%s", HELP);
			russ_conn_fatal(conn, RUSS_MSG_UNDEF_SERVICE, RUSS_EXIT_FAILURE);
			break;
		default:
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

int
main(int argc, char **argv) {
	struct russ_lis	*lis;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	lis = russ_announce(russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid()));
	if (lis == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_lis_loop(lis, NULL, alt_answer_handler, master_handler);
	exit(0);
}
