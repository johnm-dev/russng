/*
** lib/russng/russssh_server.c
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

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wait.h>

extern char **environ;

#include <russ/russ.h>

#define SSH_EXEC	"/usr/bin/ssh"
#define RUDIAL_EXEC	"/usr/bin/rudial"

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Provides access to remote host using ssh.\n"
"\n"
"/[<user>@]<host>[:<port>][<option>[...]]/... <args>\n"
"    Connect to service ... at <user>@<host>:<port> using ssh.\n"
"\n"
"    Options:\n"
"    ?controlpersist=<seconds>\n"
"        Set ControlPersist time in seconds. Default is 1.\n"
"    ?controltag=<tag>\n"
"        Used to generate a ControlPath. Required to set up control\n"
"        master functionality (if available).\n";

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
		|| (russ_env_clear() < 0)) {
		russ_sconn_fatal(sconn, "error: cannot set environment", RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* switch user */
	if (russ_switch_userinitgroups(uid, gid) < 0) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSWITCHUSER, RUSS_EXIT_FAILURE);
		exit(0);
	}
	return 0;
}

char *
dup_user_home(void) {
	struct passwd	*pw = NULL;

	if ((pw = getpwuid(getuid())) == NULL) {
		return NULL;
	}
	return strdup(pw->pw_dir);
}

int
ensure_mkdir(char *path, mode_t mode) {
	struct stat	st;

	if (stat(path, &st) < 0) {
		if (mkdir(path, mode) < 0) {
			return 0;
		}
	} else if (S_ISDIR(st.st_mode)) {
		if ((st.st_mode == 0700)
			|| (chmod (path, mode) == 0)) {
			return 0;
		}
	}
	return -1;
}

char *
escape_special(char *s) {
	char	*s2 = NULL;
	char	*a = NULL, *b = NULL;

	if ((s2 = russ_malloc(2*(strlen(s))+1)) == NULL) {
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
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*args[1024];
	char			*uhp_user = NULL, *uhp_host = NULL, *uhp_port = NULL, *uhp_opt = NULL;
	char			**opts = NULL;
	char			controlpathopt[1024], controlpersistopt[32];
	char			russssh_dirpath[1024];
	int			nargs;
	int			i, status, pid;

	sconn = sess->sconn;
	req = sess->req;

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
	if ((uhp_opt = strstr(uhp_host, "?")) != NULL) {
		*uhp_opt = '\0';
		uhp_opt++;
		/* validate tag */
		opts = russ_sarray0_new_split(uhp_opt, "?", 0);
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

	if (uhp_opt) {
		char	*controlpersist = NULL, *controltag = NULL;
		char	*user_home = NULL;

		controltag = russ_sarray0_get_suffix(opts, "controltag=");
		controlpersist = russ_sarray0_get_suffix(opts, "controlpersist=");

		user_home = dup_user_home();

		if (controltag
			&& user_home
			&& (russ_snprintf(russssh_dirpath, sizeof(russssh_dirpath), "%s/.ssh/russssh", user_home) > 0)
			&& (ensure_mkdir(russssh_dirpath, 0700) == 0)
			&& (russ_snprintf(controlpathopt, sizeof(controlpathopt), "ControlPath=%s/%%l-%%r@%%h:%%p-%s", russssh_dirpath, controltag) > 0)) {

			args[nargs++] = "-o";
			args[nargs++] = "ControlMaster=auto";
			args[nargs++] = "-o";
			args[nargs++] = controlpathopt;
			args[nargs++] = "-o";
			args[nargs++] = "ControlPersist=1";

			if (controlpersist && (russ_snprintf(controlpersistopt, sizeof(controlpersistopt), "ControlPersist=%s", controlpersist) > 0)) {
				args[nargs-1] = controlpersistopt;
			}
		}
		user_home = russ_free(user_home);
	}
	if (uhp_user) {
		args[nargs++] = "-l";
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
	if (__russ_waitpidfd(pid, &status, sconn->sysfds[0], 200) == __RUSS_WAITPIDFD_FD) {
		status = RUSS_EXIT_EXITFDCLOSED;
		kill(pid, SIGHUP);
	}
	russ_sconn_exit(sconn, WEXITSTATUS(status));
	russ_sconn_close(sconn);

	exit(0);
}

#if 0
void
svc_net_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*p = NULL, *new_spath = NULL, *userhost = NULL;
	int			i;

	sconn = sess->sconn;
	req = sess->req;

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
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

char *
get_userhostport(char *spath) {
	char	*userhostport = NULL;

	userhostport = russ_str_dup_comp(spath, '/', 1);
	return userhostport;
}

void
svc_userhostport_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*userhostport = NULL;

	sconn = sess->sconn;
	req = sess->req;

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
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*userhostport = NULL, *new_spath = NULL;

	sconn = sess->sconn;
	req = sess->req;

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
"usage: russssh_server [<conf options>]\n"
"\n"
"russ-based server for ssh-based remote connections. Configuration\n"
"can be obtained from the conf file if no options are used, otherwise\n"
"all configuration is taken from the given options.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*node = NULL;
	struct russ_svr		*svr = NULL;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_autoswitchuser(svr, 0) < 0)
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
