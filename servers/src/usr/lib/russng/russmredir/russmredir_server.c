/*
** lib/russng/russmredir_server.c
*/

/*
# license--start
#
# Copyright 2015 John Marshall
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
"Redirects connection requests.\n"
"\n"
"/... <args>\n"
"    Dial service at ... .\n";

char			*logfilename = NULL;
char			*spath_prefix = NULL;

/**
* Answer and service request only if it is for "/". Otherwise, pass
* request on with redial and splice operations.
*/
void
svc_root_handler(struct russ_sess *sess) {
	struct russ_svr		*svr = NULL;
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char	*option = NULL, **options = NULL;
	int	outfd;

	svr = sess->svr;
	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		if ((options = russ_conf_options(conf, "spaths")) == NULL) {
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
			exit(0);
		}
		outfd = sconn->fds[1];
		for (option = *options; option != NULL; option = *(++options)) {
			russ_dprintf(outfd, "%s\n", option);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

/**
* Redial and splice for service requests for /<name>/...
*/
void
svc_next_handler(struct russ_sess *sess) {
	struct russ_svr		*svr = NULL;
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*comp = NULL, *nextspath = NULL;
	char			spath[RUSS_REQ_SPATH_MAX];

	svr = sess->svr;
	sconn = sess->sconn;
	req = sess->req;

	if (((comp = russ_str_dup_comp(req->spath, '/', 1)) == NULL)
		|| ((nextspath = russ_conf_get(conf, "spaths", comp, NULL)) == NULL)) {
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			russ_lprintf(logfilename, "[%F %T] ", "req->spath (%s)\n", req->spath);
			return;
		}
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (russ_snprintf(spath, sizeof(spath), "%s%s", nextspath, &(req->spath[1+strlen(comp)])) < 0) {
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			/* fatal */
			russ_lprintf(logfilename, "[%F %T] ", "req->spath (%s)\n", req->spath);
			return;
		}
		russ_sconn_fatal(sconn, "error: spath too big", RUSS_EXIT_FAILURE);
		exit(0);
	} else {
		russ_lprintf(logfilename, "[%F %T] ", "res->path (%s) spath (%s)\n", req->spath, spath);
		req->spath = spath;
		russ_sconn_redialandsplice(sconn, RUSS_DEADLINE_NEVER, req);
		exit(0);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russmredir_server [<conf options>]\n"
"\n"
"Redirect connection by prefixing an spath.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svr		*svr = NULL;
	struct russ_svcnode	*node = NULL;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	logfilename = russ_conf_get(conf, "main", "logfile", NULL);

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_autoswitchuser(svr, 1) < 0)
		|| (russ_svr_set_matchclientuser(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)

		|| (russ_svcnode_set_handler(svr->root, svc_root_handler) < 0)

		|| ((node = russ_svcnode_add(svr->root, "*", svc_next_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_autoanswer(node, 0) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
