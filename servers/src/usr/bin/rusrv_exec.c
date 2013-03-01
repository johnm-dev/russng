/*
** bin/rusrv_exec.c
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
#include <sys/wait.h>

#include "russ_conf.h"
#include "russ.h"

char			*cgroups_home;
struct russ_conf	*conf = NULL;

char	*HELP =
"Execute a command/program.\n"
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
"To specify a cgroup in which to execute, set the cg_path attribute\n"
"to an absolute path or a path relative to the cgroups home (i.e.,\n"
"mount point).\n"
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

/*
* Find "cg_path" in connection attrv list and return the value part
* (i.e., string after the "cg_path=").
*/
char *
get_cg_path(char **attrv) {
	if (attrv) {
		for (; *attrv != NULL; attrv++) {
			if (strncmp(*attrv, "cg_path=", 8) == 0) {
				return &((*attrv)[8]);
			}
		}
	}
	return NULL;
}

void
op_execute_handler(struct russ_conn *conn) {
	struct russ_req	*req;
	FILE		*f;
	char		*cmd, **argv;
	char		*shell, *lshell, *home;
	char		*cg_path, cg_tasks_path[1024];
	pid_t		pid;
	int		argc, i, status;

	req = &(conn->req);

	/* find cg_path and set cg_tasks_path */
	if ((cg_path = get_cg_path(req->attrv)) != NULL) {
		if (cgroups_home == NULL) {
			russ_conn_fatal(conn, "error: cgroups not configured", RUSS_EXIT_FAILURE);
			return;
		} else if (strncmp(cg_path, "/", 1) != 0) {
			/* absolutize relative path */
			if (snprintf(cg_tasks_path, sizeof(cg_tasks_path), "%s/%s/tasks", cgroups_home, cg_path) < 0) {	
				russ_conn_fatal(conn, "error: bad cgroup", RUSS_EXIT_FAILURE);
				return;
			}
		} else if (snprintf(cg_tasks_path, sizeof(cg_tasks_path), "%s/tasks", cg_path) < 0) {	
			russ_conn_fatal(conn, "error: bad cgroup", RUSS_EXIT_FAILURE);
			return;
		}
	}

	/* select service */
	if (strcmp(req->spath, "/simple") == 0) {
		cmd = req->argv[0];
		argv = req->argv;
		home = "/";
	} else if ((strcmp(req->spath, "/login") == 0)
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

		if ((strcmp(req->spath, "/login") == 0)
			&& (req->argv[0] != NULL)) {
			argv[0] = lshell;
			argv[2] = req->argv[0];
		} else if ((strcmp(req->spath, "/shell") == 0)
			&& (req->argv[0] != NULL)) {
			argv[0] = shell;
			argv[2] = req->argv[0];
		} else {
			russ_conn_fatal(conn, "error: bad/missing arguments", RUSS_EXIT_FAILURE);
			exit(0);
		}
		if (argv[2] == NULL) {
			russ_conn_fatal(conn, "error: bad/missing arguments", RUSS_EXIT_FAILURE);
			exit(0);
		}
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (cgroups_home && cg_path) {
		/* migrate self process to cgroup (affects child, too) */
		if (((f = fopen(cg_tasks_path, "w")) ==  NULL)
			|| (fprintf(f, "%d", getpid()) < 0)) {
			if (f) {
				fclose(f);
			}
			russ_conn_fatal(conn, "error: could not add to cgroup", RUSS_EXIT_FAILURE);
			exit(0);
		}
		fclose(f);
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
	struct russ_req	*req;

	/* change uid/gid ASAP */
	/* TODO: this may have to move to support job service */
	if (russ_switch_user(conn->creds.uid, conn->creds.gid, 0, NULL) < 0) {
		russ_conn_fatal(conn, "error: cannot set up", RUSS_EXIT_FAILURE);
		return;
	}

	req = &(conn->req);
	switch (req->op) {
	case RUSS_OP_HELP:
		op_help_handler(conn);
		break;
	case RUSS_OP_LIST:
		op_list_handler(conn);
		break;
	case RUSS_OP_EXECUTE:
		op_execute_handler(conn);
		break;
	default:
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

	cgroups_home = russ_conf_get(conf, "job", "cgroups_home", NULL);
	lis = russ_announce(russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid()));
	if (lis == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_lis_loop(lis, NULL, NULL, master_handler);
}
