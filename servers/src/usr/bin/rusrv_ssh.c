/*
** bin/rusrv_ssh.c
*/

/*
# license--start
#
# This file is part of RUSS tools.
# Copyright (C) 2012 John Marshall
#
# RUSS tools is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

int
switch_user(struct russ_conn *conn) {
	uid_t	uid;
	gid_t	gid;

	uid = conn->cred.uid;
	gid = conn->cred.gid;

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
		russ_conn_fatal(conn, "error: cannot switch user", -1);
		exit(0);
	}
	return 0;
}

char *
escape_spaces(char *s) {
	char	*s2;
	char	*a, *b;

	if ((s2 = malloc(strlen(s)+1)) == NULL) {
		return NULL;
	}
	for (a = s, b = s2; *a != '\0'; a++, b++) {
		if (*a == ' ') {
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
			if ((args[nargs++] = escape_spaces(conn->req.attrv[i])) == NULL) {
				russ_conn_fatal(conn, "error: out of memory", RUSS_EXIT_FAILURE);
				russ_conn_close(conn);
				exit(0);
			}
		}
	}
	args[nargs++] = conn->req.op;
	args[nargs++] = new_spath;
	if ((conn->req.argv != NULL) && (conn->req.argv[0] != NULL)) {
		for (i = 0; conn->req.argv[i] != NULL; i++) {
			if ((args[nargs++] = escape_spaces(conn->req.argv[i])) == NULL) {
				russ_conn_fatal(conn, "error: out of memory", RUSS_EXIT_FAILURE);
				russ_conn_close(conn);
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
		exit(-1);
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
	struct russ_request	*req;
	int			outfd;
	int			i;

	outfd = conn->fds[1];
	req = &(conn->req);
	if (index(&req->spath[1], '/') > 0) {
		/* /.../ */
		svc_x_handler(conn);
	} else if (strcmp(req->op, "execute") == 0) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	} else if (strcmp(req->op, "help") == 0) {
        	russ_dprintf(outfd, "%s", HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else if (strcmp(req->op, "list") == 0) {
#if 0
		if (strcmp(req->spath, "/") == 0) {
			russ_dprintf(conn->fds[1], "dial\n");
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		}
#endif
		//russ_conn_fatal(conn, RUSS_MSG_UNSPEC_SERVICE, RUSS_EXIT_SUCCESS);
		russ_conn_fatal(conn, "error: unspecified service", RUSS_EXIT_SUCCESS);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
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
"all configuration is taken from the given options.\n"
"\n"
"Where:"
"-h <hostsfile>\n"
"    Set up /hosts/* and /hid/* services.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(-1);
	}

	lis = russ_announce(russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid()));
	if (lis == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_listener_loop(lis, master_handler);
	exit(0);
}
