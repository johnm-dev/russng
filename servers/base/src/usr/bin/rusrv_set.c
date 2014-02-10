/*
** bin/rusrv_set.c
*/

/*
# license--start
#
# Copyright 2013 John Marshall
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

#define DEFAULT_DIAL_TIMEOUT	(30000)

struct russ_conf	*conf = NULL;
const char		*HELP =
"Intermediate service to set/modify attributes and positional\n"
"arguments in dial request. Multiple settings can be made by\n"
"separating them by the spath separator (/). The period (.) is used\n"
"to indicate no more settings to make.\n"
"\n"
"/index=value/..././...\n"
"    Set positional argument 'index' to 'value'. There must be a\n"
"    at the index position. An index of -1 will append the value to\n"
"    the argument list (i.e., argv).\n"
"\n"
"/name=value/..././...\n"
"    Set attribute 'name' to 'value'. An existing name assignment in\n"
"    the attribute list (i.e., attrv) will be overwritten. Otherwise\n"
"    a new entry of 'name=value' will be added.\n";

/**
* Update attrv or argv request string array.
*
* String argument formats:
* index=value to update argv[index] to value; if index is -1, the
*     value is appended to the argv; invalid indexes will result in
*     and error
* name=value to update attrv to "name=value"; update attrv if
*     "name=" is found; otherwise add to attrv.
*
* TODO: there is currently no limit on the size of argv and attrv in
*     terms of elements or memory size. However, the spath itself is
*     limited.
*
* @param req		request object
* @param s		string
* @return		0 on success; -1 on failure
*/
int
update_attrv_argv(struct russ_req *req, char *s) {
	char	pref[64], *p;
	int	index;

	if (((p = strchr(s, '=')) == NULL)
		|| ((p+1)-s >= sizeof(pref))
		|| (strncpy(pref, s, (p+1)-s) < 0)) {
		return -1;
	}
	pref[(p+1)-s] = '\0';
	if (sscanf(pref, "%d=", &index) == 1) {
		/* index=value; index==-1 to append */
		if ((index < -1) || (russ_sarray0_update(&req->argv, index, p+1) < 0)) {
			return -1;
		}

	} else {
		/* name=value */
		index = russ_sarray0_find_prefix(req->attrv, pref);
		if (russ_sarray0_update(&req->attrv, index, s) < 0) {
			return -1;
		}
	}
	return 0;
}

void
svc_root_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_LIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_root_value_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	int			attrsz, argvsz;
	char			*spath = NULL, *p0 = NULL, *p1 = NULL;
	ssize_t			n;

	if ((spath = strdup(req->spath)) == NULL) {
		goto failed_update;
	}

	/* extract settings; look for /., update req->spath */
	p0 = spath;
	do {
		if ((p1 = strchr(p0+1, '/')) != NULL) {
			*p1 = '\0';
		}
		if (strcmp(p0, "/.") == 0) {
			/* end marker */
			if (p1 == NULL) {
				p1 = "/";
			} else {
				*p1 = '/';
			}
			req->spath = russ_free(req->spath);
			req->spath = p1;
			break;
		}
		if (req->opnum == RUSS_OPNUM_EXECUTE) {
			if (update_attrv_argv(req, p0+1) < 0) {
				goto failed_update;
			}
		}
		if (p1 != NULL) {
			*p1 = '/';
		}
		p0 = p1;
	} while (p1 != NULL);

	/* forward to next service (if possible) */
	if (p1 != NULL) {
		russ_sconn_redial_and_splice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
		exit(0);
	}
	spath = russ_free(spath);
	russ_standard_answer_handler(sconn);
	return;

failed_update:
	spath = russ_free(spath);
	if (russ_standard_answer_handler(sconn) == 0) {
		russ_sconn_fatal(sconn, "error: could not set attribute/argument", RUSS_EXIT_FAILURE);
	}
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_set [<conf options>]\n"
"\n"
"Set/modify attributes and positional arguments.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*root, *node;
	struct russ_svr		*svr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "*", svc_root_value_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}

	if (russ_svr_announce(svr,
		russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0666),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
