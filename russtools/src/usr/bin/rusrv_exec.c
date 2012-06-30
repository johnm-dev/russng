/*
** bin/rusrv_exec.c
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

#include <errno.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "russ.h"

char	*HELP =
"Execute a command/program.\n"
"\n"
"/job <cgroup> <cmd>\n"
"    Execute a shell command string with a configured login shell\n"
"    within a cgroup.\n"
"\n"
"/login <cmd>\n"
"    Execute a shell command string with a configured login shell.\n"
"\n"
"/shell <cmd>\n"
"    Execute a shell command in an unconfigured (not a login) shell.\n"
"\n"
"/simple <path> [<arg> ...]\n"
"    Execute a program with arguments directly (no shell).\n"
"\n"
"All services use the given attribute settings to configure the\n"
"environment and are applied before shell settings.\n";

/*
* Return shell path, shell path with - prefix (indicating login),
* and user home. Free shell, lshell, and home when finished.
*/
int
get_user_info(uid_t uid, char **shell, char **lshell, char **home) {
	struct passwd	*pwd;
	char		*_shell = NULL, *_lshell = NULL, *_home = NULL;

	if ((pwd = getpwuid(uid)) == NULL) {
		return -1;
	}
	if (((_shell = strdup(pwd->pw_shell)) == NULL)
		|| ((_lshell = malloc(strlen(pwd->pw_shell)+1+1)) == NULL)
		|| (strcpy(&(_lshell[1]), pwd->pw_shell) == NULL)
		|| ((_home = strdup(pwd->pw_dir)) == NULL)) {
		goto free_strings;
	}
	_lshell[0] = '-';
	*shell = _shell;
	*lshell = _lshell;
	*home = _home;

	return 0;
free_strings:
	free(_shell);
	free(_lshell);
	free(_home);

	return -1;
}

void
op_execute_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	char			*cmd, **argv;
	char			*shell, *lshell, *home;
	int			argc, i;

	req = &(conn->req);

	/* select service */
	if (strcmp(req->spath, "/simple") == 0) {
		cmd = req->argv[0];
		argv = req->argv;
		home = "/";
	} else {
		/* argv[] = {shell, "-c", cmd, NULL} */
		if ((argv = malloc(sizeof(char *)*4)) == NULL) {
			russ_dprintf(conn->fds[2], "error: could not run\n");
			return -1;
		}

		if (get_user_info(getuid(), &shell, &lshell, &home) < 0) {
			russ_dprintf(conn->fds[2], "error: could not get user/shell info\n");
			return -1;
		}
		cmd = shell;
		argv[1] = "-c";
		argv[2] = req->argv[0];
		argv[3] = NULL;

		if (strcmp(req->spath, "/job") == 0) {
			argv[0] = lshell;
		} else if (strcmp(req->spath, "/login") == 0) {
			argv[0] = lshell;
		} else if (strcmp(req->spath, "/shell") == 0) {
			argv[0] = shell;
		} else {
			russ_dprintf(conn->fds[2], "error: no service available\n");
			return -1;
		}
	}

	/* TODO: setup for cgroups */
	if (strcmp(req->spath, "/job") == 0) {
		russ_dprintf(conn->fds[2], "error: no service available\n");
		return -1;
	}

	/* TODO: set minimal settings:
	*	env (LOGNAME, USER, HOME)
	*	cwd
	*	umask
	*/
	chdir(home);
	umask(0);

	/* move and close fds */
	dup2(conn->fds[0], 0);
	dup2(conn->fds[1], 1);
	dup2(conn->fds[2], 2);
	close(conn->fds[0]);
	close(conn->fds[1]);
	close(conn->fds[2]);
	for (i = 3; i < 128; i++) {
		close(i);
	}

	/* execute */
	execve(cmd, argv, req->attrv);

	/* on error */
	russ_conn_fatal(conn, "error: could not execute program", RUSS_EXIT_FAILURE);
}

void
op_help_handler(struct russ_conn *conn) {
	russ_dprintf(conn->fds[1], "%s", HELP);
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
op_list_handler(struct russ_conn *conn) {
	if (strcmp(conn->req.spath, "/") == 0) {
		russ_dprintf(conn->fds[1], "/job\n/login\n/shell\n/simple\n");
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
}

void
svc_handler(struct russ_conn *conn) {
	struct russ_request	*req;

	/* change uid/gid ASAP */
	/* TODO: this may have to move to support job service */
	if ((setgid(getgid()) < 0)
		|| (setuid(getuid()) < 0)) {
		russ_conn_fatal(conn, "error cannot set up", RUSS_EXIT_FAILURE);
		return;
	}

	req = &(conn->req);
	if (strcmp(req->op, "help") == 0) {
		op_help_handler(conn);
	} else if (strcmp(req->op, "list") == 0) {
		op_list_handler(conn);
	} else if (strcmp(req->op, "execute") == 0) {
		op_execute_handler(conn);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_SYS_FAILURE);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_exec <saddr>\n"
"\n"
"russ-based exec server to execute programs.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;
	char			*saddr;

	signal(SIGCHLD, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(-1);
	}
	saddr = argv[1];

	if ((lis = russ_announce(saddr, 0666, getuid(), getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_listener_loop(lis, svc_handler);
}
