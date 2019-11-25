/*
** lib/russng/russredir_server.c
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

#include <grp.h>
#include <pwd.h>
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

char			*logfilename = NULL;

char *
gid2groupname(gid_t gid) {
	struct group	*gr = NULL;

	if ((gr = getgrgid(gid)) == NULL) {
		return NULL;
	}
	return strdup(gr->gr_name);
}

char *
uid2username(uid_t uid) {
	struct passwd	*pw = NULL;

	if ((pw = getpwuid(uid)) == NULL) {
		return NULL;
	}
	return strdup(pw->pw_name);
}

/**
* Answer and service request only if it is for "/". Otherwise, pass
* request on with redial and splice operations.
*/
void
svc_root_handler(struct russ_sess *sess) {
	struct russ_svr		*svr = NULL;
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			spath[RUSS_REQ_SPATH_MAX];
	gid_t			gid;
	uid_t			uid;
	char			*groupname, *username;
	char			groupsecname[64], usersecname[64];
	char			*prefix = NULL;
	int			n;

	svr = sess->svr;
	sconn = sess->sconn;
	req = sess->req;

	gid = sconn->creds.gid;
	uid = sconn->creds.uid;
	groupname = gid2groupname(gid);
	username = uid2username(uid);

	if ((groupname == NULL) || (username == NULL)) {
		russ_sconn_fatal(sconn, "error: cannot find username or groupname", RUSS_EXIT_FAILURE);
		exit(0);
	}

	if ((russ_snprintf(groupsecname, sizeof(groupsecname), "group.%s", groupname) < 0)
		|| (russ_snprintf(usersecname, sizeof(usersecname), "user.%s", username) < 0)) {
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			russ_sconn_fatal(sconn, "error: out of memory", RUSS_EXIT_FAILURE);
		}
		exit(0);	
	}

	if (((prefix = russ_conf_get(conf, usersecname, "spath", NULL)) == NULL)
		&& ((prefix = russ_conf_get(conf, groupsecname, "spath", NULL)) == NULL)
		&& ((prefix = russ_conf_get(conf, "user", "spath", NULL)) == NULL)
		&& ((prefix = russ_conf_get(conf, "next", "spath", NULL)) == NULL)) {
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			russ_sconn_fatal(sconn, "error: cannot find next spath", RUSS_EXIT_FAILURE);
		}
		exit(0);
	}

	if (russ_snprintf(spath, sizeof(spath), "%s%s", prefix, req->spath) < 0) {
		if ((svr->answerhandler == NULL) || (svr->answerhandler(sconn) < 0)) {
			russ_sconn_fatal(sconn, "error: out of memory", RUSS_EXIT_FAILURE);
		}
		exit(0);
	}

	//russ_lprintf(logfilename, "[%F %T] ", "res->path (%s) spath (%s)\n", req->spath, spath);
	req->spath = spath;
	russ_sconn_redialandsplice(sconn, RUSS_DEADLINE_NEVER, req);
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russredir_server [<conf options>]\n"
"\n"
"Redirect connection by prefixing an spath. The prefix is selected by\n"
"username, groupname, or default as specified in the sections of the\n"
"configuration file.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svr		*svr = NULL;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_load(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	//logfilename = russ_conf_get(conf, "main", "logfile", NULL);

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_autoswitchuser(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)

		|| (russ_svcnode_set_handler(svr->root, svc_root_handler) < 0)
		|| (russ_svcnode_set_virtual(svr->root, 1) < 0)
		|| (russ_svcnode_set_autoanswer(svr->root, 0) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
