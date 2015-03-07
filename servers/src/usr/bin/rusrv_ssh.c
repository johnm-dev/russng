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

#include <russ.h>

#define SSH_EXEC	"/usr/bin/ssh"
#define RUDIAL_EXEC	"/usr/bin/rudial"

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Provides access to remote host using ssh.\n"
"\n"
"/[<user>@]<host>[:<port>]/... <args>\n"
"    Connect to service ... at <user>@<host>:<port> using ssh.\n";

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
switch_user(struct russ_sconn *sconn) {
	uid_t	uid;
	gid_t	gid;

	uid = sconn->creds.uid;
	gid = sconn->creds.gid;

#if 0
	if (uid == 0) {
		russ_sconn_fatal(sconn, "error: cannot run for root (uid of 0)", -1);
		exit(0);
	}
#endif

	/* set up env */
	if ((chdir("/") < 0)
		|| (clearenv() < 0)) {
		russ_sconn_fatal(sconn, "error: cannot set environment", RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* switch user */
	if (russ_switch_user(uid, gid, 0, NULL) < 0) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSWITCHUSER, RUSS_EXIT_FAILURE);
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
execute(struct russ_sess *sess, char *userhost, char *new_spath) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char	*args[1024];
	int	nargs;
	char	*uhp_user, *uhp_host, *uhp_port;
	int	i, status, pid;

	switch_user(sconn);

	/* parse out user, host, and port */
	uhp_user = strdup(userhost);
	if ((uhp_host = strstr(uhp_user, "@")) != NULL) {
		*uhp_host = '\0';
		uhp_host++;
	} else {
		uhp_host = uhp_user;
		uhp_user = NULL;
	}
	if ((uhp_port = strstr(uhp_host, ":")) != NULL) {
		*uhp_port = '\0';
		uhp_port++;
	}

	/* build args array */
	nargs = 0;
	args[nargs++] = SSH_EXEC;
	args[nargs++] = "-o";
	args[nargs++] = "StrictHostKeyChecking=no";
	args[nargs++] = "-o";
	args[nargs++] = "BatchMode=yes";
	args[nargs++] = "-o";
	args[nargs++] = "LogLevel=QUIET";
	if (uhp_user) {
		args[nargs++] = "-u";
		args[nargs++] = uhp_user;
	}
	if (uhp_port) {
		args[nargs++] = "-p";
		args[nargs++] = uhp_port;
	}
	args[nargs++] = uhp_host;
	args[nargs++] = RUDIAL_EXEC;
	if ((req->attrv != NULL) && (req->attrv[0] != NULL)) {
		for (i = 0; req->attrv[i] != NULL; i++) {
			args[nargs++] = "-a";
			if ((args[nargs++] = escape_special(req->attrv[i])) == NULL) {
				russ_sconn_fatal(sconn, "error: out of memory", RUSS_EXIT_FAILURE);
				exit(0);
			}
		}
	}
	args[nargs++] = req->op;
	args[nargs++] = new_spath;
	if ((req->argv != NULL) && (req->argv[0] != NULL)) {
		for (i = 0; req->argv[i] != NULL; i++) {
			if ((args[nargs++] = escape_special(req->argv[i])) == NULL) {
				russ_sconn_fatal(sconn, "error: out of memory", RUSS_EXIT_FAILURE);
				exit(0);
			}
		}
	}
	args[nargs++] = NULL;

#if 0
	{
		char **xargs;
		for (xargs = args; *xargs != NULL; xargs++) {
			fprintf(stderr, "(%s)\n", *xargs);
		}
	}
#endif

	/* fix up fds and exec */
	signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == 0) {
		/* dup sconn stdin/out/err fds to standard stdin/out/err */
		if ((dup2(sconn->fds[0], 0) >= 0) &&
			(dup2(sconn->fds[1], 1) >= 0) &&
			(dup2(sconn->fds[2], 2) >= 0)) {

			russ_sconn_close(sconn);
			execv(args[0], args);
		}

		/* should not get here! */
		russ_dprintf(2, "error: could not execute\n");
		exit(1);
	}
	/* close sconn stdin/out/err; leave exitfd */
	russ_close(sconn->fds[0]);
	russ_close(sconn->fds[1]);
	russ_close(sconn->fds[2]);

	/* wait for exit value, pass back, and close up */
	waitpid(pid, &status, 0);
	russ_sconn_exit(sconn, WEXITSTATUS(status));
	russ_sconn_close(sconn);

	exit(0);
}

#if 0
void
svc_net_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char	*p, *new_spath, *userhost;
	int	i;

	/* extract and validate user@host and new_spath */
	userhost = &(req->spath[5]);
	if ((p = index(userhost, '/')) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	new_spath = strdup(p);
	p[0] = '\0'; /* terminate userhost */

	execute(sess, userhost, new_spath);
}
#endif

void
svc_root_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

char *
get_userhostport(char *spath) {
	char	*userhostport;

	userhostport = russ_str_dup_comp(spath, '/', 1);
	return userhostport;
}

void
svc_userhostport_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*userhostport;

	if ((userhostport = get_userhostport(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
	userhostport = russ_free(userhostport);
}

/**
* Handler for /<user@host:port>/...
*
* @param sess		session object
*/
void
svc_userhostport_other_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*userhostport, *new_spath;

	if ((userhostport = get_userhostport(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	new_spath = strchr(req->spath+1, '/');
	execute(sess, userhostport, new_spath);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_ssh [<conf options>]\n"
"\n"
"russ-based server for ssh-based remote connections. Configuration\n"
"can be obtained from the conf file if no options are used, otherwise\n"
"all configuration is taken from the given options.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*node;
	struct russ_svr		*svr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)

		|| (russ_svcnode_set_handler(svr->root, svc_root_handler) < 0)

		|| ((node = russ_svcnode_add(svr->root, "*", svc_userhostport_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_userhostport_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
	}
	russ_svr_loop(svr);
	exit(0);
}
