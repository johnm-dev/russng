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

#include <russ.h>

struct russ_conf	*conf = NULL;
const char		*HELP =
"Echoes lines of input back.\n";

void
svc_root_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			buf[1024];
	ssize_t			n;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		/* serve the input from fd passed to client */
		while ((n = russ_read(sconn->fds[0], buf, sizeof(buf))) > 0) {
			russ_writen(sconn->fds[1], buf, n);
		}
		exit(0);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_echo [<conf options>]\n"
"\n"
"Russ-based echo server.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*root;
	struct russ_svr		*svr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK, RUSS_SVR_LIS_SD_DEFAULT)) == NULL)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
