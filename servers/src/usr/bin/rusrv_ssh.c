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
#include <unistd.h>

extern char **environ;

#include "configparser.h"
#include "russ.h"

/* global */
struct configparser	*config;

char	*HELP = 
"Provides access to remote user@host using ssh.\n"
"\n"
"/user@host/... <args>\n"
"    Connect to service ... at user@host using ssh.\n";

#define SSH_EXEC	"/usr/bin/ssh"
#define RUDIAL_EXEC	"/usr/bin/rudial"

void
svc_ssh_handler(struct russ_conn *conn) {
	char	*args[1024];
	int	nargs;
	char	*p, *new_spath, *userhost;
	uid_t	uid;
	gid_t	gid;
	int	i, n, status, pid;

	uid = conn->cred.uid;
	gid = conn->cred.gid;

	if (uid == 0) {
		russ_dprintf(conn->fds[2], "error: cannot run for root (uid of 0)\n");
		russ_conn_exit(conn, -1);
		exit(0);
	}

	/* switch user */
	setgid(gid);
	setuid(uid);

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[1];
	if ((p = index(userhost, '/')) == NULL) {
		russ_dprintf(conn->fds[2], "error: cannot run for root (uid of 0)\n");
		russ_conn_exit(conn, -1);
		exit(0);
	}
	new_spath = strdup(p);
	p[0] = '\0'; /* terminate userhost */

	/* build args array */
	nargs = 0;
	args[nargs++] = SSH_EXEC;
	args[nargs++] = userhost;
	args[nargs++] = RUDIAL_EXEC;
	if ((conn->req.attrv != NULL) && (conn->req.attrv[0] != NULL)) {
		for (i = 0; conn->req.attrv[i] != NULL; i++) {
			args[nargs++] = "-a";
			args[nargs++] = conn->req.attrv[i];
		}
	}
	args[nargs++] = conn->req.op;
	args[nargs++] = new_spath;
	if ((conn->req.argv != NULL) && (conn->req.argv[0] != NULL)) {
		for (i = 0; conn->req.argv[i] != NULL; i++) {
			args[nargs++] = conn->req.argv[i];
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

	fprintf(stderr, "status (%d) exit status (%d)\n", status, WEXITSTATUS(status));
	russ_conn_exit(conn, WEXITSTATUS(status));
	russ_conn_close(conn);
	exit(0);
}

void
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;

	req = &(conn->req);
	if ((strlen(req->spath) > 1) && (strncmp(req->spath, "/", 1) == 0)) {
		svc_ssh_handler(conn);
	} else {
		if (strcmp(req->op, "help") == 0) {
        		russ_dprintf(conn->fds[1], "%s", HELP);
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
		}
	}
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_ssh <conf>\n"
"\n"
"russ-based server for ssh-based remote connections.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;
	char			*filename, *path;
	int			mode, uid, gid;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(-1);
	}

	filename = argv[1];
	if ((config = configparser_read(filename)) == NULL) {
		fprintf(stderr, "error: could not read config file\n");
		exit(-1);
	}

	mode = configparser_getsint(config, "server", "mode", 0600);
	uid = configparser_getint(config, "server", "uid", getuid());
	gid = configparser_getint(config, "server", "gid", getgid());
	path = configparser_get(config, "server", "path", NULL);
	if ((lis = russ_announce(path, mode, uid, gid)) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_listener_loop(lis, master_handler);
}
