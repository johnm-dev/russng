/*
** lib/russng/russpass_server.c
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

#include <russ/russ.h>

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Pass request to another service with splicing of fds.\n"
"\n"
"/<spath> <args>\n"
"    Dial service at <spath>.\n";

/**
* Answer and service request only if it is for "/". Otherwise, pass
* request on with redial and splice operations.
*/
void
svc_root_handler(struct russ_sess *sess) {
	struct russ_svr		*svr = NULL;
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;

	svr = sess->svr;
	sconn = sess->sconn;
	req = sess->req;

	if (strcmp(req->spath, "/") == 0) {
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			/* fatal */
			return;
		}
		if (req->opnum == RUSS_OPNUM_HELP) {
			russ_dprintf(sconn->fds[1], "%s", HELP);
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
			russ_sconn_close(sconn);
		}
	} else {
		russ_sconn_redialandsplice(sconn, RUSS_DEADLINE_NEVER, req);
		exit(0);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russpass_server [<conf options>]\n"
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
	struct russ_svcnode	*root = NULL;
	struct russ_svr		*svr = NULL;

	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_autoswitchuser(svr, 1) < 0)
		|| (russ_svr_set_matchclientuser(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)

		|| (russ_svcnode_set_handler(svr->root, svc_root_handler) < 0)
		|| (russ_svcnode_set_autoanswer(svr->root, 0) < 0)
		|| (russ_svcnode_set_virtual(svr->root, 1) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
