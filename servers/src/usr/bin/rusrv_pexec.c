/*
** bin/rusrv_pexec.c
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

#include "russ_conf.h"
#include "russ.h"

/* globals */
int			gl_argc;
char			**gl_argv;
struct russ_conf	*conf = NULL;

char	*HELP =
"Personal execution server to quickly run a command/program in a\n"
"preloaded login environment. Preloading the environment eliminates\n"
"the environment configuration delay.\n"
"\n"
"exec <arg> [...]\n"
"    Execute command in a login configured environment. Attributes\n"
"    are used to augment the environment prior to execution.\n"
"\n"
"reload\n"
"    Reload login environment. Attributes are used to augment the\n"
"    environment after the reload. Useful for when the settings for\n"
"    a login environment have changed.\n"
"\n"
"shutdown\n"
"    Shut down server.\n"
"\n"
"status\n"
"    Return the status of the server.\n";

/**
* Augment the environemnt.
*
* @param attrv	List of "name=value" strings.
*/
void
augment_env(char **attrv) {
	int	i;

	for (i = 0; attrv[i] != NULL; i++) {
		putenv(attrv[i]);
	}
}

/**
* Duplicate argc and argv to gl_argc and gl_argv, respectively.
*
* @param argc	main() argc
* @param argv	main() argv
* @return	0 on success, -1 on failure
*/
int
dup_argc_argv_to_global(int argc, char **argv) {
	int	i;

	if ((gl_argv = malloc(sizeof(char *)*(argc+1))) == NULL) {
		return -1;
	}
	for (i = 0; i < argc; i++) {
		gl_argv[i] = argv[i];
	}
	gl_argv[i] = NULL;
	gl_argc = argc;
	return 0;
}

/**
* Close a range of fds.
*
* @param low_fd	value of lowest fd to close
* @param hi_fd	value of highest fd to close
*/
void
close_fds(int low_fd, int hi_fd) {
	int	i;

	for (i = low_fd; i <= hi_fd; i++) {
		close(i);
	}
}

void
op_execute_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	char			*shell, *lshell, *home;
	int			argc, i;

	req = &(conn->req);

	/* select service */
	if (strcmp(req->spath, "/exec") == 0) {
		;
	} else if (strcmp(req->spath, "/reload") == 0) {
		if (fork() == 0) {
			augment_env(req->attrv);
			close_fds(0, 127);
			execv(gl_argv[0], gl_argv);
		}
		kill(getppid(), SIGTERM);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		return;
	} else if (strcmp(req->spath, "/shutdown") == 0) {
		kill(getppid(), SIGTERM);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		return;
	} else if (strcmp(req->spath, "/status") == 0) {
		russ_dprintf(conn->fds[1], "ok\n");
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		return;
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		return;
	}
	
	/* move and close fds */
	dup2(conn->fds[0], 0);
	dup2(conn->fds[1], 1);
	dup2(conn->fds[2], 2);
	close(conn->fds[0]);
	close(conn->fds[1]);
	close(conn->fds[2]);
	close_fds(3, 127);

	/* augment and execute */
	augment_env(req->attrv);
	execvp(req->argv[0], req->argv);

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
		russ_dprintf(conn->fds[1], "exec\nreload\nshutdown\nstatus\n");
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
	if ((setgid(getgid()) < 0)
		|| (setuid(getuid()) < 0)) {
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
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_pexec [<conf options>] [-- -ttl <timeout>]\n"
"\n"
"Russ-based personal exec server to execute programs.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	/* dup argc and argv to globals for possible later reference */
	if (dup_argc_argv_to_global(argc, argv) < 0) {
		fprintf(stderr, "error: cannot save argc and argv\n");
		exit(-1);
	}

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
