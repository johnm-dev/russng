/*
** lib/russng/russalive_server.c
*/

/*
# license--start
#
# Copyright 2012-2013 John Marshall
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

#ifdef RUSSALIVE_THREAD
	#include <pthread.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <russ/russ.h>

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Alive server.\n"
"\n"
"/...\n"
"    Return exit value of 0 (success).\n";

void
svc_root_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
#ifdef RUSSALIVE_FORK
		exit(0);
#endif
#ifdef RUSSALIVE_THREAD
		pthread_exit(0);
#endif
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russalive_server [<conf options>]\n"
"\n"
"russ-based server for indicated it is \"alive\".\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*node = NULL;
	struct russ_svr		*svr = NULL;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((svr = russ_init(conf)) == NULL)
#ifdef RUSSALIVE_FORK
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
#endif
#ifdef RUSSALIVE_THREAD
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_THREAD) < 0)
#endif
		|| (russ_svr_set_autoswitchuser(svr, 0) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)

		|| (russ_svcnode_set_handler(svr->root, svc_root_handler) < 0)
		|| (russ_svcnode_set_virtual(svr->root, 1) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
	}
	russ_svr_loop(svr);
	exit(0);
}
