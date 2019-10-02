/*
** lib/russng/russplus_server.c
*/

/*
# license--start
#
# Copyright 2012-2019 John Marshall
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

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#include <russ/russ.h>

int
cmpstrp(const void *p0, const void *p1) {
	return strcmp(*(char * const *)p0, *(char * const *)p1);
}

int
print_dir_list(struct russ_sconn *sconn, char *dirpath) {
	struct stat		st;
	struct dirent		*dent = NULL;
	DIR			*dir = NULL;
	char			**names;
	char			path[RUSS_REQ_SPATH_MAX];
	int			errfd, outfd;
	int			cap, i, n;

	outfd = sconn->fds[1];
	errfd = sconn->fds[2];

	if ((dir = opendir(dirpath)) == NULL) {
		russ_dprintf(errfd, "error: cannot open directory\n");
		return -1;
	} else {
		cap = 2048;
		n = 0;
		names = malloc(sizeof(char *)*cap);

		while ((dent = readdir(dir)) != NULL) {
			if ((strcmp(dent->d_name, ".") == 0)
				|| (strcmp(dent->d_name, "..") == 0)) {
				continue;
			}
			if ((russ_snprintf(path, sizeof(path), "%s/%s", dirpath, dent->d_name) < 0)
				|| (lstat(path, &st) < 0)) {
				/* problem */
				continue;
			}
			if (S_ISDIR(st.st_mode)
				|| S_ISSOCK(st.st_mode)
				|| russ_is_conffile(path)
				|| S_ISLNK(st.st_mode)) {
				if ((n == cap) || ((names[n++] = strdup(dent->d_name)) == NULL)) {
					russ_dprintf(errfd, "error: too many entries\n");
					cap = -1;
					closedir(dir);
					goto free_names;
				}
			}
		}
		closedir(dir);

		/* sort and output */
		qsort(names, n, sizeof(char *), cmpstrp);
		for (i = 0; i < n; i++) {
			russ_dprintf(outfd, "%s\n", names[i]);
		}
	}
free_names:
	for (; n >= 0; n--) {
		free(names[n]);
	}
	free(names);
	if (cap < 0) {
		return -1;
	}
	return 0;
}

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Plus server handling \"+\" component in the service path. Passes\n"
"the request to another service with splicing of fds.\n"
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
	struct stat		st;
	char			spath[RUSS_REQ_SPATH_MAX];
	char			*saddr = NULL;

	svr = sess->svr;
	sconn = sess->sconn;
	req = sess->req;

	russ_snprintf(spath, sizeof(spath), "%s/%s", RUSS_SERVICES_DIR, &req->spath[1]);
	if ((stat(spath, &st) == 0) && (!S_ISSOCK(st.st_mode)) && (!russ_is_conffile(spath))) {
		/* non-socket, non-conffile file or directory */
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			/* fatal */
			return;
		}

		if (req->opnum == RUSS_OPNUM_HELP) {
			russ_dprintf(sconn->fds[1], HELP);
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		} else if (req->opnum == RUSS_OPNUM_LIST) {
			if (S_ISDIR(st.st_mode)) {
				if (print_dir_list(sconn, spath) == 0) {
					russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
				} else {
					russ_sconn_exit(sconn, RUSS_EXIT_FAILURE);
				}
			}
		} else {
			russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		}
		russ_sconn_close(sconn);
		exit(0);
	} else {
		req->spath = russ_free(req->spath);
		req->spath = strdup(spath);

		russ_sconn_redialandsplice(sconn, RUSS_DEADLINE_NEVER, req);
	}
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russplus_server [<conf options>]\n"
"\n"
"Searches for services under multiple locations, in order:\n"
"1) ~/.russ/services 2) /run/russ/services. Then, dials and passes\n"
"fds of the dialed service back to the original client.\n"
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
