/*
** bin/rusrv_hello3.c
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

#include <stdio.h>
#include <stdlib.h>

#include <russ.h>

void
svc_root_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
}

void
svc_chm_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*msg;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		if (strcmp(req->spath, "/confused") == 0) {
			msg = "hello world, where am I?\n";
		} else if (strcmp(req->spath, "/happy") == 0) {
			msg = "hello world, where am I?\n";
		} else if (strcmp(req->spath, "/mean") == 0) {
			msg = "hello world, where am I?\n";
		}
		russ_dprintf(sconn->fds[1], msg);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*root;
	struct russ_svr		*svr;

	if (argc != 2) {
		fprintf(stderr, "usage: rusrv_hello <server_path>\n");
		exit(1);
	}
	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| (russ_svcnode_add(root, "confused", svc_chm_handler) == NULL)
		|| (russ_svcnode_add(root, "happy", svc_chm_handler) == NULL)
		|| (russ_svcnode_add(root, "mean", svc_chm_handler) == NULL)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)
		|| (russ_svr_set_auto_switch_user(svr, 1) < 0)
		|| (russ_svr_set_help(svr, "Hello world example server.\n") < 0)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}
	if (russ_svr_announce(svr, argv[1], 0666, -1, -1) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
