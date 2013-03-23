/*
** bin/rusrv_ssh.c
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
"Provides access to remote host using ssh.\n"
"[<user>@]<host>[:<port>]/... <args>\n"
"    Connect to service ... at <user>@<host>:<port> using ssh.\n";

#define SSH_EXEC	"/usr/bin/ssh"
#define RUDIAL_EXEC	"/usr/bin/rudial"

#if defined(__APPLE__) || defined(__FreeBSD__)
/*
** simple, minimal replacement for clearenv which does not actually
** free the environ strings; use recommended for forking situations
** only
*/
int
clearenv(void) {
	*environ = NULL;
	return 0;
}
#endif

int
switch_user(struct russ_conn *conn) {
	uid_t	uid;
	gid_t	gid;

	uid = conn->creds.uid;
	gid = conn->creds.gid;

#if 0
	if (uid == 0) {
		russ_conn_fatal(conn, "error: cannot run for root (uid of 0)", -1);
		exit(0);
	}
#endif

	/* set up env */
	if ((chdir("/") < 0)
		|| (clearenv() < 0)) {
		russ_conn_fatal(conn, "error: cannot set environment", RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* switch user */
	if (russ_switch_user(uid, gid, 0, NULL) < 0) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SWITCH_USER, RUSS_EXIT_FAILURE);
		exit(0);
	}
	return 0;
}

char *
escape_special(char *s) {
	char	*s2;
	char	*a, *b;

	if ((s2 = malloc(2*(strlen(s))+1)) == NULL) {
		return NULL;
	}
	for (a = s, b = s2; *a != '\0'; a++, b++) {
		switch (*a) {
		case ' ':
		case '\'':
		case '$':
		case '`':
		case '"':
		default:
			*b = '\\';
			b++;
		}
		*b = *a;
	}
	*b = '\0';
	return s2;
}

void
execute(struct russ_conn *conn, char *userhost, char *new_spath) {
	char	*args[1024];
	int	nargs;
	char	op_str[256];
	int	i, status, pid;

	switch_user(conn);

	/* build args array */
	nargs = 0;
	args[nargs++] = SSH_EXEC;
	args[nargs++] = "-o";
	args[nargs++] = "StrictHostKeyChecking=no";
	args[nargs++] = "-o";
	args[nargs++] = "BatchMode=yes";
	args[nargs++] = "-o";
	args[nargs++] = "LogLevel=QUIET";
	args[nargs++] = userhost;
	args[nargs++] = RUDIAL_EXEC;
	if ((conn->req.attrv != NULL) && (conn->req.attrv[0] != NULL)) {
		for (i = 0; conn->req.attrv[i] != NULL; i++) {
			args[nargs++] = "-a";
			if ((args[nargs++] = escape_special(conn->req.attrv[i])) == NULL) {
				russ_conn_fatal(conn, "error: out of memory", RUSS_EXIT_FAILURE);
				exit(0);
			}
		}
	}
	if (snprintf(op_str, sizeof(op_str), "%u", conn->req.op) < 0) {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
		exit(0);
	}
	args[nargs++] = op_str;
	args[nargs++] = new_spath;
	if ((conn->req.argv != NULL) && (conn->req.argv[0] != NULL)) {
		for (i = 0; conn->req.argv[i] != NULL; i++) {
			if ((args[nargs++] = escape_special(conn->req.argv[i])) == NULL) {
				russ_conn_fatal(conn, "error: out of memory", RUSS_EXIT_FAILURE);
				exit(0);
			}
		}
	}
	args[nargs++] = NULL;

	/* fix up fds and exec */
	signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == 0) {
		dup2(conn->fds[0], 0);
		dup2(conn->fds[1], 1);
		dup2(conn->fds[2], 2);
		close(conn->fds[3]);
		execv(args[0], args);

		/* should not get here! */
		russ_dprintf(conn->fds[2], "error: could not execute\n");
		exit(1);
	}
	close(conn->fds[0]);
	close(conn->fds[1]);
	close(conn->fds[2]);
	waitpid(pid, &status, 0);

	russ_conn_exit(conn, WEXITSTATUS(status));
	russ_conn_close(conn);
	exit(0);
}

#if 0
void
svc_net_handler(struct russ_conn *conn) {
	char	*p, *new_spath, *userhost;
	int	i;

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[5];
	if ((p = index(userhost, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	new_spath = strdup(p);
	p[0] = '\0'; /* terminate userhost */

	execute(conn, userhost, new_spath);
}
#endif

void
svc_x_handler(struct russ_conn *conn) {
	char	*p, *new_spath, *userhost;
	int	i;

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[1];
	if ((p = index(userhost, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	new_spath = strdup(p);
	p[0] = '\0'; /* terminate userhost */

	execute(conn, userhost, new_spath);
}

/*
*/
void
master_handler(struct russ_conn *conn) {
	struct russ_req	*req;
	int		outfd;
	int		i;

	outfd = conn->fds[1];
	req = &(conn->req);
	if (index(&req->spath[1], '/') > 0) {
		/* /.../ */
		svc_x_handler(conn);
	} else {
		switch (req->op) {
		case RUSS_OP_EXECUTE:
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			break;
		case RUSS_OP_HELP:
	        	russ_dprintf(outfd, "%s", HELP);
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			break;
		case RUSS_OP_LIST:
#if 0
			if (strcmp(req->spath, "/") == 0) {
				russ_dprintf(conn->fds[1], "dial\n");
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			}
#endif
			//russ_conn_fatal(conn, RUSS_MSG_UNSPEC_SERVICE, RUSS_EXIT_SUCCESS);
			russ_conn_fatal(conn, "error: unspecified service", RUSS_EXIT_SUCCESS);
			break;
		default:
			russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
		}
	}
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_ssh [<conf options>] [-- <hostsfile>]\n"
"\n"
"russ-based server for ssh-based remote connections. Configuration\n"
"can be obtained from the conf file if no options are used, otherwise\n"
"all configuration is taken from the given options.\n";
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
	russ_lis_loop(lis, NULL, NULL, master_handler);
	exit(0);
}
