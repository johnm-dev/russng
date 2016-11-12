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
#include <fcntl.h>
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

#include <russ.h>

#define CONTAINER_TYPE_NONE	0
#define CONTAINER_TYPE_CGROUP	1
#define CONTAINER_TYPE_JOBID	2

struct container {
	int	type;
	/* values */
	char	path[4096];
	long	id;
};

char			*cgroup_base, *cgroup_spath;
struct russ_conf	*conf = NULL;
struct container	cont;
const char		*HELP =
"Execute a command/program.\n"
"\n"
"/cgroup/<cgname>/login\n"
"/cgroup/<cgname>/shell\n"
"/cgroup/<cgname>/simple\n"
"    Execute using login, shell, or simple methods within a cgroup\n"
"    container. cgname is used to identify the cgroup to use if\n"
"    a cg_path attribute is not specified (in which case the cgname\n"
"    value is ignored). Note: cgname cannot contain / characters."
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
"environment and are applied before shell settings. For security\n"
"reasons, some attributes are not passed to the environment (e.g.,\n"
"LD_PRELOAD).\n";

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
		|| ((_lshell = russ_malloc(strlen(pwd->pw_shell)+1+1)) == NULL)
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
	_username = russ_free(_username);
	_shell = russ_free(_shell);
	_lshell = russ_free(_lshell);
	_home = russ_free(_home);

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
	int		rv;

	pamc.conv = &misc_conv;
	pamc.appdata_ptr = NULL;

	if ((rv = pam_start(service_name, username, &pamc, &pamh)) != PAM_SUCCESS) {
		goto pam_end;
	}

	if (((rv = pam_open_session(pamh, PAM_SILENT)) != PAM_SUCCESS)
		|| ((rv = pam_close_session(pamh, PAM_SILENT)) != PAM_SUCCESS)) {
		goto pam_end;
	}

pam_end:
	pam_end(pamh, rv);
	return (rv == PAM_SUCCESS ? 0: 1);
}
#else
int
setup_by_pam(char *service_name, char *username) {
	return 0;
}
#endif /* USE_PAM */

int
add_pid_cgroup(int pid, char *cg_path) {
	if (cgroup_spath) {
		char	*argv[3];
		char	pidst[16];
		int	n, exitst;

		if (((n = snprintf(pidst, sizeof(pidst), "%d", pid)) < 0)
			|| (n > sizeof(pidst))) {
			return -1;
		}

		argv[0] = pidst;
		argv[1] = cg_path;
		argv[2] = NULL;
		if ((russ_dialv_wait(russ_to_deadline(30000), "execute", cgroup_spath, NULL, argv, &exitst) < 0)
			|| (exitst != 0)) {
			return -1;
		}
	} else if (cgroup_base) {
		char	path[8192];
		int	n;
		int	fd;

		if (((n = snprintf(path, sizeof(path), "%s/%s/cgroup.procs", cgroup_base, cg_path)) < 0)
			|| (n > sizeof(path))) {
			return -1;
		}

		/* migrate self process to cgroup (affects child, too) */
		if (((fd = open(path, O_WRONLY)) < 0)
		       || (russ_dprintf(fd, "%d", pid) < 0)) {
		       if (fd >= 0) {
			       close(fd);
			}
			return -1;

		}
		fd = russ_close(fd);
	} else {
		return -1;
	}
	return 0;
}

char *
find_cgroup_base(void) {
	struct stat	st;
	char		*paths, *base, *next;

	if ((paths = russ_conf_get(conf, "cgroup", "base", NULL)) == NULL) {
		return NULL;
	}
	base = paths;
	do {
		if ((next = strchr(base, ':')) != NULL) {
			*next = '\0';
		}
		if ((stat(base, &st) == 0) && (S_ISDIR(st.st_mode))) {
			base = strdup(base);
			goto free_paths;
		}
		base = next+1;
	} while (next != NULL);
	/* fallback to initial path; assumes \0 at :s in paths */
	base = strdup(paths);
free_paths:
	paths = russ_free(paths);
	return base;
}

char *
squote_string(char *s) {
	char	*s2;

	if (((s2 = russ_malloc(strlen(s)+1+2)) == NULL)
		|| (sprintf(s2, "'%s'", s) < 0)) {
		return NULL;
	}
	return s2;
}

void
str_replace(char *s, char oldc, char newc) {
	for (; *s != '\0'; s++) {
		if (*s == oldc) {
			*s = newc;
		}
	}
}

/*
* Find "cg_path" in connection attrv list and return a copy of the
* value part (i.e., string after the "cg_path=") which must be
* freed by caller.
*/
char *
get_cg_path(char **attrv) {
	if (attrv) {
		for (; *attrv != NULL; attrv++) {
			if (strncmp(*attrv, "cg_path=", 8) == 0) {
				return strdup(&((*attrv)[8]));
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
	if (((envp2 = russ_malloc(sizeof(char *)*(3+count))) == NULL)
		|| (memset(envp2, 3+count, sizeof(char *)) == NULL)) {
		goto free_envp2;
	}
	if (((envp2[0] = russ_malloc(7+1+strlen(username)+1)) == NULL)
		|| ((envp2[1] = russ_malloc(4+1+strlen(username)+1)) == NULL)
		|| ((envp2[2] = russ_malloc(4+1+strlen(home)+1)) == NULL)) {
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
	envp2[0] = russ_free(envp2[0]);
	envp2[1] = russ_free(envp2[1]);
	envp2[2] = russ_free(envp2[2]);
free_envp2:
	envp2 = russ_free(envp2);
	return NULL;
}

void
execute(struct russ_sess *sess, char *cwd, char *username, char *home, char *cmd, char **argv, char **envp) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			**envp2;
	pid_t			pid;
	int			fd;
	int			status;

	if (setup_by_pam("rusrv_exec", username) < 0) {
		russ_sconn_fatal(sconn, "error: could not set up for user", RUSS_EXIT_FAILURE);
		return;
	}

	/* change uid/gid ASAP */
	/* TODO: this may have to move to support job service */
	if (russ_switch_userinitgroups(sconn->creds.uid, sconn->creds.gid) < 0) {
		russ_sconn_fatal(sconn, "error: cannot set up", RUSS_EXIT_FAILURE);
		return;
	}

	switch (cont.type) {
	case CONTAINER_TYPE_NONE:
		break;
	case CONTAINER_TYPE_CGROUP:
		if (add_pid_cgroup(getpid(), cont.path) < 0) {
			russ_sconn_fatal(sconn, "error: could not add to cgroup", RUSS_EXIT_FAILURE);
			exit(0);
		}
		break;
	default:
		russ_sconn_fatal(sconn, "error: unknown container type", RUSS_EXIT_FAILURE);
		return;
	}


	if ((envp2 = dup_envp_plus(envp, username, home)) == NULL) {
		russ_sconn_fatal(sconn, "error: could not set up env", RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* TODO: set minimal settings:
	*	umask
	*/
	chdir("/");
	umask(022);

	/* execute */
	signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == 0) {
		/* dup sconn stdin/out/err fds to standard stdin/out/err */
		if ((dup2(sconn->fds[0], 0) >= 0) &&
			(dup2(sconn->fds[1], 1) >= 0) &&
			(dup2(sconn->fds[2], 2) >= 0)) {

			setsid();
			chdir(cwd);
			russ_sconn_close(sconn);
			execve(cmd, argv, envp2);
		}

		/* should not get here! */
		russ_dprintf(2, "error: could not execute\n");
		exit(1);
	}
	/* close sconn stdin/out/err; leave exitfd */
	russ_close(sconn->fds[0]);
	russ_close(sconn->fds[1]);
	russ_close(sconn->fds[2]);

	/* wait for exit value; pass back, and close up */
	waitpid(pid, &status, 0);
	russ_sconn_exit(sconn, WEXITSTATUS(status));
	russ_sconn_close(sconn);

	exit(0);
}

void
svc_loginshell_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			**argv;
	char			*username, *shell, *lshell, *home;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		if (req->argv[0] == NULL) {
			russ_sconn_fatal(sconn, "error: bad/missing arguments", RUSS_EXIT_FAILURE);
			exit(0);
		}
		/* argv[] = {shell, "-c", cmd, NULL} */
		if ((argv = russ_malloc(sizeof(char *)*4)) == NULL) {
			russ_sconn_fatal(sconn, "error: could not run", RUSS_EXIT_FAILURE);
			exit(0);
		}

		if (get_user_info(sconn->creds.uid, &username, &shell, &lshell, &home) < 0) {
			russ_sconn_fatal(sconn, "error: could not get user/shell info", RUSS_EXIT_FAILURE);
			exit(0);
		}

		argv[0] = shell;
		argv[1] = "-c";
		argv[2] = req->argv[0];
		argv[3] = NULL;

		if (strcmp(req->spath, "/login") == 0) {
			argv[0] = lshell;
		}

		execute(sess, home, username, home, shell, argv, req->attrv);
		russ_sconn_exit(sconn, RUSS_EXIT_FAILURE);
		exit(0);
	}
}

void
svc_simple_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*username, *shell, *lshell, *home;

	if (get_user_info(sconn->creds.uid, &username, &shell, &lshell, &home) < 0) {
		russ_sconn_fatal(sconn, "error: could not get user/shell info", RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		execute(sess, "/", username, home, req->argv[0], req->argv, req->attrv);
		russ_sconn_exit(sconn, RUSS_EXIT_FAILURE);
		exit(0);
	}
}

void
svc_cgroup_path_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	;
}

void
svc_cgroup_path_loginshellsimple_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*cg_path = NULL;
	char			*next_spath = NULL;
	int			n;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		/* find cg_path */
		if ((cg_path = get_cg_path(req->attrv)) == NULL) {
			if ((cg_path = russ_str_dup_comp(req->spath, '/', 2)) == NULL) {
				russ_sconn_fatal(sconn, "error: bad cgroup", RUSS_EXIT_FAILURE);
				exit(0);
			}
			str_replace(cg_path, ':', '/');
		}

		/* create container object */
		cont.type = CONTAINER_TYPE_CGROUP;
		if (((n = snprintf(cont.path, sizeof(cont.path), "%s", cg_path)) < 0)
			|| (n > sizeof(cont.path))) {
			exit(0);
		}

		/* patch request spath */
		if (((next_spath = strchr(req->spath+1, '/')) == NULL)
			|| ((next_spath = strchr(next_spath+1, '/')) == NULL)
			|| ((next_spath = strdup(next_spath)) == NULL)) {
			russ_sconn_exit(sconn, RUSS_EXIT_FAILURE);
			exit(0);
		}
		req->spath = russ_free(req->spath); /* assumes dynamic allocation */
		req->spath = next_spath;

		/* forward to "next" handler */
		if ((strcmp(req->spath, "/shell") == 0) || (strcmp(req->spath, "/login") == 0)) {
			svc_loginshell_handler(sess);
		} else if (strcmp(req->spath, "/simple") == 0) {
			svc_simple_handler(sess);
		} else {
			russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
			exit(0);
		}
	}
}

void
svc_cgroup_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
		exit(0);
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
	struct russ_svcnode	*node;
	struct russ_svr		*svr;

	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	/* container info initialization */
	cont.type = CONTAINER_TYPE_NONE;
	if ((cgroup_base = find_cgroup_base()) == NULL) {
		fprintf(stderr, "warning: cannot find cgroup base\n");
	}
	if ((cgroup_spath = russ_conf_get(conf, "cgroup", "spath", NULL)) == NULL) {
		fprintf(stderr, "warning: cannot find cgroup spath\n");
	}

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)
		|| ((node = russ_svcnode_add(svr->root, "cgroup", svc_cgroup_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_cgroup_path_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_add(node, "login", svc_cgroup_path_loginshellsimple_handler) == NULL)
		|| (russ_svcnode_add(node, "shell", svc_cgroup_path_loginshellsimple_handler) == NULL)
		|| (russ_svcnode_add(node, "simple", svc_cgroup_path_loginshellsimple_handler) == NULL)
		|| ((node = russ_svcnode_add(svr->root, "login", svc_loginshell_handler)) == NULL)
		|| ((node = russ_svcnode_add(svr->root, "shell", svc_loginshell_handler)) == NULL)
		|| ((node = russ_svcnode_add(svr->root, "simple", svc_simple_handler)) == NULL)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
