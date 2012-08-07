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
#include <sys/wait.h>

#include "russ_conf.h"
#include "russ.h"

struct russ_conf	*conf = NULL;
char	*HELP =
"Execute a command/program.\n"
"\n"
"job <cgroup> <cmd>\n"
"    Execute a shell command string with a configured login shell\n"
"    within a cgroup.\n"
"\n"
"login <cmd>\n"
"    Execute a shell command string with a configured login shell.\n"
"\n"
"shell <cmd>\n"
"    Execute a shell command in an unconfigured (not a login) shell.\n"
"\n"
"simple <path> [<arg> ...]\n"
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

char *
squote_string(char *s) {
	char	*s2;

	if (((s2 = malloc(strlen(s)+1+2)) == NULL)
		|| (sprintf(s2, "'%s'", s) < 0)) {
		return NULL;
	}
	return s2;
}

void
op_execute_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	FILE			*f;
	char			*cmd, **argv;
	char			*shell, *lshell, *home;
	char			cgpath[1024];
	pid_t			pid;
	int			argc, i, status;

	req = &(conn->req);

	/* select service */
	if (strcmp(req->spath, "/simple") == 0) {
		cmd = req->argv[0];
		argv = req->argv;
		home = "/";
	} else if ((strcmp(req->spath, "/job") == 0)
		|| (strcmp(req->spath, "/login") == 0)
		|| (strcmp(req->spath, "/shell") == 0)) {
		/* argv[] = {shell, "-c", cmd, NULL} */
		if ((argv = malloc(sizeof(char *)*4)) == NULL) {
			russ_conn_fatal(conn, "error: could not run", RUSS_EXIT_FAILURE);
			return;
		}

		if (get_user_info(getuid(), &shell, &lshell, &home) < 0) {
			russ_conn_fatal(conn, "error: could not get user/shell info", RUSS_EXIT_FAILURE);
			return;
		}
		cmd = shell;
		argv[1] = "-c";
		/* argv[2] is set below */
		argv[3] = NULL;

		if ((strcmp(req->spath, "/job") == 0)
			&& (req->argv[0] != NULL)
			&& (req->argv[1] != NULL)) {
			argv[0] = lshell;
			argv[2] = squote_string(req->argv[1]);

			/* setup for cgroups */
			if ((snprintf(cgpath, sizeof(cgpath), "%s/tasks", req->argv[0]) < 0)
				|| ((f = fopen(cgpath, "w")) ==  NULL)
				|| (fprintf(f, "%d", getpid()) < 0)) {
				fclose(f);
				russ_conn_fatal(conn, "error: could not add to cgroup", RUSS_EXIT_FAILURE);
				russ_conn_close(conn);
				exit(0);
			}
			fclose(f);
		} else if ((strcmp(req->spath, "/login") == 0)
			&& (req->argv[0] != NULL)) {
			argv[0] = lshell;
			argv[2] = squote_string(req->argv[0]);
		} else if ((strcmp(req->spath, "/shell") == 0)
			&& (req->argv[0] != NULL)) {
			argv[0] = shell;
			argv[2] = squote_string(req->argv[0]);
		} else {
			russ_conn_fatal(conn, "error: bad/missing arguments", RUSS_EXIT_FAILURE);
			russ_conn_close(conn);
			exit(0);
		}
		if (argv[2] == NULL) {
			russ_conn_fatal(conn, "error: bad/missing arguments", RUSS_EXIT_FAILURE);
			russ_conn_close(conn);
			exit(0);
		}
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		russ_conn_close(conn);
		exit(0);
	}

	/* TODO: set minimal settings:
	*	env (LOGNAME, USER, HOME)
	*	cwd
	*	umask
	*/
	chdir(home);
	umask(0);

	/* execute */
	signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == 0) {
		dup2(conn->fds[0], 0);
		dup2(conn->fds[1], 1);
		dup2(conn->fds[2], 2);
		close(conn->fds[3]);
		//for (i = 3; i < 128; i++) {
			//close(i);
		//}
		execve(cmd, argv, req->attrv);

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

void
op_help_handler(struct russ_conn *conn) {
	russ_dprintf(conn->fds[1], "%s", HELP);
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
op_list_handler(struct russ_conn *conn) {
	if (strcmp(conn->req.spath, "/") == 0) {
		russ_dprintf(conn->fds[1], "job\nlogin\nshell\nsimple\n");
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
}

void
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;

	/* change uid/gid ASAP */
	/* TODO: this may have to move to support job service */
	if (russ_switch_user(conn->cred.uid, conn->cred.gid, 0, NULL) < 0) {
		russ_conn_fatal(conn, "error: cannot set up", RUSS_EXIT_FAILURE);
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
"usage: rusrv_exec [<conf options>]\n"
"\n"
"russ-based exec server to execute programs.\n"
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
}
