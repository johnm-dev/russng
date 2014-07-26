/*
** bin/rusrv_tee.c
*/

/*
# license--start
#
# Copyright 2014 John Marshall
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

#include <fcntl.h>
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

#include <russ.h>

#define DEFAULT_DIAL_TIMEOUT	(30000)
#define BUFSIZE		(1<<15)
#define BUFSIZE_MAX	(1<<20)

/* global */
struct russ_conf	*conf = NULL;
int			fds[RUSS_CONN_NFDS];
const char		*HELP = 
"Captures data transferred between client and server.\n"
"\n"
"/<attr>/...\n"
"    The attribute from which to get information on how to capture\n"
"    (or not) data being transferred. The '...' is the spath for\n"
"    the next server(s) to connect to.\n"
"\n"
"The attribute value must take one of the following forms:\n"
"fixed:<path0>:...\n"
"    Specifies fixed paths for connection fds, where path0\n"
"    corresponds to fd 0, etc. Specify empty paths to not capture.\n";

char			*FUTURE_HELP = "dir:<path>\n"
"    Specifies a directory path under which files are created and\n"
"    to which data is saved. Filenames are <timestamp>-<fd>.\n";

void
init_fds(void) {
	int	i;

	for (i = 0; i < RUSS_CONN_NFDS; i++) {
		fds[i] = -1;
	}
}

int
open_fds(char *value) {
	char	path[1024];
	int	i, fd;

	for (i = 0; i < RUSS_CONN_NFDS; i++) {
		if (russ_str_get_comp(value, ':', i+1, path, sizeof(path)) < 0) {
			/* no more */
			break;
		} else if (path[0] != '\0') {
			if ((fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
				/* failed */
				break;
			}
			fds[i] = fd;
		}
	}
	return i;
}

void
close_fds(void) {
	int	i;

	for (i = 0; i < RUSS_CONN_NFDS; i++) {
		if (fds[i] >= 0) {
			close(fds[i]);
			fds[i] = -1;
		}
	}
}

void
tee_callback(struct russ_relaystream *self, int dir, void *cbarg) {
	russ_deadline	last;
	int		i;

	if (dir == 0) {
		/* only capture on input */
		i = (int)cbarg;
		if (russ_writen(fds[i], self->rbuf->data, self->rbuf->len) < 0) {
			/* problem */
			/* remove from fds */
			close(fds[i]);
			fds[i] = -1;
		}
	}
}

/**
* Handler for the / service.
*
* Provides HELP.
*
* @param sess		session object
*/
void
svc_root_handler(struct russ_sess *sess) {
	/* auto handling in svr */
}

/**
* Handler for the /count service.
*
* Output the number of targets.
*
* @param sess		session object
*/
void
svc_attr_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	struct russ_cconn	*cconn = NULL;
	char			*spath;
	char			*attr, *attreq;
	char			*name, *value;
	int			i, attrlen, exit_status;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		spath = req->spath;
		if (((attr = russ_str_dup_comp(spath, '/', 1)) == NULL)
			|| ((attrlen = strlen(attr)) < 0)
			|| ((attreq = malloc(attrlen+2)) == NULL)
			|| (sprintf(attreq, "%s=", attr) < 0)) {
			goto badattr;
		}
		if (req->attrv == NULL) {
			goto badattr;
		}

		for (i = 0, name = req->attrv[i]; name != NULL; i++, name = req->attrv[i]) {
			if (strncmp(attreq, name, attrlen+1) == 0) {
				break;
			}
		}
		if (name == NULL) {
			goto badattr;
		}
		if ((value = strdup(name+attrlen+1)) == NULL) {
			goto badattr;
		}

		/* TODO: remove attr from attrv */
		spath = req->spath+attrlen+1;
		if ((cconn = russ_dialv(russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req->op, spath, req->attrv, req->argv)) == NULL) {
			russ_sconn_fatal(sconn, "error: cannot connect", RUSS_EXIT_FAILURE);
			exit(0);
		}

		if (strncmp(value, "fixed:", 6) == 0) {
			if (open_fds(value) < 0) {
				russ_sconn_fatal(sconn, "error: cannot set up capture files", RUSS_EXIT_FAILURE);
				exit(0);
			}
		} else {
			russ_sconn_fatal(sconn, "error: unspecified capture files", RUSS_EXIT_FAILURE);
			exit(0);
		}

		{
			struct russ_relay	*relay;
			int			bufsize = BUFSIZE;
			int			x;

			relay = russ_relay_new(3);
			x = russ_relay_add_with_callback(relay, sconn->fds[0], cconn->fds[0], bufsize, 1,
				(fds[0] >= 0) ? tee_callback : NULL, (void *)0);
			russ_relay_add_with_callback(relay, cconn->fds[1], sconn->fds[1], bufsize, 1,
				(fds[1] >= 0) ? tee_callback : NULL, (void *)1);
			russ_relay_add_with_callback(relay, cconn->fds[2], sconn->fds[2], bufsize, 1,
				(fds[2] >= 0) ? tee_callback : NULL, (void *)2);

			cconn->fds[0] = -1;
			cconn->fds[1] = -1;
			cconn->fds[2] = -1;
			russ_relay_serve(relay, -1, cconn->sysfds[RUSS_CONN_SYSFD_EXIT]);
			if (russ_cconn_wait(cconn, -1, &exit_status) < 0) {
				fprintf(stderr, "%s\n", RUSS_MSG_BADCONNEVENT);
				exit_status = RUSS_EXIT_SYSFAILURE;
			}
		}

		close_fds();

		russ_cconn_close(cconn);
		cconn = russ_cconn_free(cconn);

		russ_sconn_exit(sconn, exit_status);
		russ_sconn_close(sconn);
		sconn = russ_sconn_free(sconn);
	}

	exit(0);
badattr:
	russ_sconn_fatal(sconn, "error: cannot find attribute", RUSS_EXIT_FAILURE);
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_tee [<conf options>]\n"
"\n"
"Captures data transfer between client and server.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*root, *node;
	struct russ_svr		*svr;

	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_init(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	init_fds();

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "*", svc_attr_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		//|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK, RUSS_SVR_LIS_SD_DEFAULT)) == NULL)
		|| (russ_svr_set_auto_switch_user(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);	
}
