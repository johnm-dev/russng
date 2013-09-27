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
#ifdef USE_PAM
#include <security/pam_appl.h>
#include <security/pam_misc.h>
#endif /* USE_PAM */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "russ_conf.h"
#include "russ.h"

char			*cgroups_home;
struct russ_conf	*conf = NULL;

char	*HELP =
"Execute a command/program.\n"
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
"To specify a cgroup in which to execute, set the cg_path attribute\n"
"to an absolute path or a path relative to the cgroups home (i.e.,\n"
"mount point).\n"
"\n"
"All services use the given attribute settings to configure the\n"
"environment and are applied before shell settings.\n";

/*
* Given a uid, return username, shell path, shell path with - prefix
* (indicating login), and user home.
*
* Caller must free username, shell, lshell, and home when finished.
*
* @param uid		user id
* @param username	place to store username reference
* @param shell		place to store shell reference
* @param lshell		place to store shell (with -) reference
* @param home		place to store home reference
* @return		0 on success; -1 on error
*/
int
get_user_info(uid_t uid, char **username, char **shell, char **lshell, char **home) {
	struct passwd	*pwd;
	char		*_username = NULL, *_home = NULL;
	char		*_shell = NULL, *_lshell = NULL;

	if ((pwd = getpwuid(uid)) == NULL) {
		return -1;
	}
	if (((_username = strdup(pwd->pw_name)) == NULL)
		|| ((_shell = strdup(pwd->pw_shell)) == NULL)
		|| ((_lshell = malloc(strlen(pwd->pw_shell)+1+1)) == NULL)
		|| (strcpy(&(_lshell[1]), pwd->pw_shell) == NULL)
		|| ((_home = strdup(pwd->pw_dir)) == NULL)) {
		goto free_strings;
	}
	*username = _username;
	_lshell[0] = '-';
	*shell = _shell;
	*lshell = _lshell;
	*home = _home;

	return 0;
free_strings:
	free(_username);
	free(_shell);
	free(_lshell);
	free(_home);

	return -1;
}

/**
* Set according to pam service settings.
*
* This is primarily to hook into limits, env so that the execution
* environment of the process is like a login.
*
* @param service_name	service name as found under /etc/pam.d
* @param username	user name
* @return		0 for success; -1 for failure
*/
#ifdef USE_PAM
int
setup_by_pam(char *service_name, char *username) {
	pam_handle_t	*pamh;
	struct pam_conv	pamc;

	pamc.conv = &misc_conv;
	pamc.appdata_ptr = NULL;

	if (pam_start(service_name, username, &pamc, &pamh) != PAM_SUCCESS) {
		goto shutdown_pam;
	}
#if 0
	if (pam_acct_mgmt(pamh, PAM_SILENT) != PAM_SUCCESS) {
		goto shutdown_pam;
	}
#endif
	if ((pam_open_session(pamh, PAM_SILENT) != PAM_SUCCESS)
		|| (pam_close_session(pamh, PAM_SILENT) != PAM_SUCCESS)) {
		goto shutdown_pam;
	}

#if 0
{
	struct rlimit	rlim;
	getrlimit(RLIMIT_NOFILE, &rlim);
	fprintf(stderr, "no file hard (%ld) soft (%ld)\n", (long)rlim.rlim_max, (long)rlim.rlim_cur);
}
#endif

	pam_end(pamh, 0);
	return 0;
shutdown_pam:
	pam_end(pamh, 0);
	return -1;
}
#else
int
setup_by_pam(char *service_name, char *username) {
	return 0;
}
#endif /* USE_PAM */

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

/**
* Duplicate envp and enhance with LOGNAME, USER, and HOME.
*
* @param envp		initial env settings
* @param username	user name
* @param home		home path
* @return		duplicated and enhanced envp; NULL on failure
*/
char **
dup_envp_plus(char **envp, char *username, char *home) {
	char	*_envp[1] = {NULL};
	char	**envp2;
	int	i, count;

	/* count items (including NULL) */
	if (envp == NULL) {
		envp = _envp;
	}
	for (count = 0; envp[count] != NULL; count++);
	count++;

	/* allocate space (including for LOGNAME, USER, HOME) */
	if (((envp2 = malloc(sizeof(char *)*(3+count))) == NULL)
		|| (memset(envp2, 3+count, sizeof(char *)) == NULL)) {
		goto free_envp2;
	}
	if (((envp2[0] = malloc(7+1+strlen(username)+1)) == NULL)
		|| ((envp2[1] = malloc(4+1+strlen(username)+1)) == NULL)
		|| ((envp2[2] = malloc(4+1+strlen(home)+1)) == NULL)) {
		goto free_envp2_items;
	}

	/* set and copy members */
	if ((sprintf(envp2[0], "LOGNAME=%s", username) < 0)
		|| (sprintf(envp2[1], "USER=%s", username) < 0)
		|| (sprintf(envp2[2], "HOME=%s", home) < 0)) {
		goto free_envp2_items;
	}
	for (i = 0; i < count; i++) {
		envp2[3+i] = envp[i];

	}
	return envp2;

free_envp2_items:
	free(envp2[0]);
	free(envp2[1]);
	free(envp2[2]);
free_envp2:
	free(envp2);
	return NULL;
}

void
execute(struct russ_sess *sess, char *cwd, char *username, char *home, char *cmd, char **argv, char **envp) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	FILE			*f;
	char			*cg_path, cg_tasks_path[1024];
	char			**envp2;
	pid_t			pid;
	int			status;

	if (setup_by_pam("rusrv_exec", username) < 0) {
		russ_conn_fatal(conn, "error: could not set up for user", RUSS_EXIT_FAILURE);
		return;
	}

	/* change uid/gid ASAP */
	/* TODO: this may have to move to support job service */
	if (russ_switch_user(conn->creds.uid, conn->creds.gid, 0, NULL) < 0) {
		russ_conn_fatal(conn, "error: cannot set up", RUSS_EXIT_FAILURE);
		return;
	}

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

	if ((envp2 = dup_envp_plus(envp, username, home)) == NULL) {
		russ_conn_fatal(conn, "error: could not set up env", RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* TODO: set minimal settings:
	*	umask
	*/
	chdir("/");
	umask(0);

	/* execute */
	signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == 0) {
		/* dup conn stdin/out/err fds to standard stdin/out/err */
		if ((dup2(conn->fds[0], 0) >= 0) &&
			(dup2(conn->fds[1], 1) >= 0) &&
			(dup2(conn->fds[2], 2) >= 0)) {

			chdir(cwd);
			russ_conn_close(conn);
			execve(cmd, argv, envp2);
		}

		/* should not get here! */
		russ_dprintf(2, "error: could not execute\n");
		exit(1);
	}
	/* close conn stdin/out/err; leave exitfd */
	russ_close(conn->fds[0]);
	russ_close(conn->fds[1]);
	russ_close(conn->fds[2]);

	/* wait for exit value; pass back, and close up */
	waitpid(pid, &status, 0);
	russ_conn_exit(conn, WEXITSTATUS(status));
	russ_conn_close(conn);

	exit(0);
}

void
svc_root_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;

	switch (req->opnum) {
	case RUSS_OPNUM_HELP:
		russ_dprintf(conn->fds[1], "%s", HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

void
svc_login_shell_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	char			**argv;
	char			*username, *shell, *lshell, *home;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		if (req->argv[0] == NULL) {
			russ_conn_fatal(conn, "error: bad/missing arguments", RUSS_EXIT_FAILURE);
			return;
		}
		/* argv[] = {shell, "-c", cmd, NULL} */
		if ((argv = malloc(sizeof(char *)*4)) == NULL) {
			russ_conn_fatal(conn, "error: could not run", RUSS_EXIT_FAILURE);
			return;
		}

		if (get_user_info(conn->creds.uid, &username, &shell, &lshell, &home) < 0) {
			russ_conn_fatal(conn, "error: could not get user/shell info", RUSS_EXIT_FAILURE);
			return;
		}

		argv[0] = shell;
		argv[1] = "-c";
		argv[2] = req->argv[0];
		argv[3] = NULL;

		if (strcmp(req->spath, "/login") == 0) {
			argv[0] = lshell;
		}

		execute(sess, home, username, home, shell, argv, req->attrv);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	russ_conn_exit(conn, RUSS_EXIT_FAILURE);
	exit(0);
}

void
svc_simple_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	char			*username, *shell, *lshell, *home;

	if (get_user_info(conn->creds.uid, &username, &shell, &lshell, &home) < 0) {
		russ_conn_fatal(conn, "error: could not get user/shell info", RUSS_EXIT_FAILURE);
		return;
	}

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		execute(sess, "/", username, home, req->argv[0], req->argv, req->attrv);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
	russ_conn_exit(conn, RUSS_EXIT_FAILURE);
	exit(0);
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
	struct russ_svcnode	*root, *node;
	struct russ_svr		*svr;

	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "login", svc_login_shell_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "shell", svc_login_shell_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "simple", svc_simple_handler)) == NULL)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}

	cgroups_home = russ_conf_get(conf, "job", "cgroups_home", NULL);
	if (russ_svr_announce(svr,
		russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
