/*
** bin/rusrv_pexec.c
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
#if 0
"reload\n"
"    Reload login environment. Attributes are used to augment the\n"
"    environment after the reload. Useful for when the settings for\n"
"    a login environment have changed.\n"
"\n"
#endif
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

/**
* Set up a clean environment for programs to be execute in.
*
* @return		0 on success; -1 on failure
*/
int
setup_clean_environment(void) {
	fprintf(stderr, "error: setup_clean_environment() is NIY\n");
	return -1;
}

void
svc_root_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;

	switch (conn->req.opnum) {
	case RUSS_OPNUM_HELP:
		russ_dprintf(conn->fds[1], "%s", HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

/**
* Handler for the /exec service.
*
* Executes the command with args in a preloaded environment.
*
* @param sess		session object
*/
void
svc_exec_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	char			*shell, *lshell, *home;
	int			argc, i;

	switch(conn->req.opnum) {
	case RUSS_OPNUM_EXECUTE:
		/* move and close fds */
		dup2(conn->fds[0], 0);
		dup2(conn->fds[1], 1);
		dup2(conn->fds[2], 2);
		close(conn->fds[0]);
		close(conn->fds[1]);
		close(conn->fds[2]);
		close_fds(3, 127);

		/* augment and execute */
		augment_env(conn->req.attrv);
		execvp(conn->req.argv[0], conn->req.argv);

		/* on error */
		russ_conn_fatal(conn, "error: could not execute program", RUSS_EXIT_FAILURE);
		break;
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

#if 0
/**
* Handler for the /reload service.
*
* TODO:
* This is currently not implemented properly so that the new server
* with reloaded environment is actually serving the socket.
*
* @param sess		session object
*/
void
svc_reload_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;

	switch (conn->req.opnum) {
	case RUSS_OPNUM_EXECUTE:
		if (fork() == 0) {
			augment_env(conn->req.attrv);
			close_fds(0, 127);
			execv(gl_argv[0], gl_argv);
		}
		kill(getppid(), SIGTERM);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}
#endif

/**
* Handler for the /shutdown service.
*
* Shuts down the parent (server).
*
* @param sess		session object
*/
void
svc_shutdown_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;

	switch (conn->req.opnum) {
	case RUSS_OPNUM_EXECUTE:
		kill(getppid(), SIGTERM);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

/**
* Handler for the /status service.
*
* An "ok" indicates that the server is running.
*
* @param sess		session object
*/
void
svc_status_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;

	switch (conn->req.opnum) {
	case RUSS_OPNUM_EXECUTE:
		russ_dprintf(conn->fds[1], "ok\n");
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
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
	struct russ_svcnode	*root, *node;
	struct russ_svr		*svr;

	signal(SIGPIPE, SIG_IGN);

	/* dup argc and argv to globals for possible later reference */
	if (dup_argc_argv_to_global(argc, argv) < 0) {
		fprintf(stderr, "error: cannot save argc and argv\n");
		exit(1);
	}

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "exec", svc_exec_handler)) == NULL)
		//|| ((node = russ_svcnode_add(root, "reload", svc_reload_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "shutdown", svc_shutdown_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "status", svc_status_handler)) == NULL)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}

	if (russ_svr_announce(svr,
		russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}

	/* TODO: implement setup_clean_environment() */
	if (setup_clean_environment() < 0) {
		exit(11);
	}

	russ_svr_loop(svr);
	exit(0);
}
