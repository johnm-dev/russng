/*
** lib/russng/russssh2_server.c
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

#include <russ/priv.h>

#define SSH_EXEC	"/usr/bin/ssh"
#define RUDIAL_EXEC	"/usr/bin/rudial"
#define RUTUNS_EXEC	"/usr/bin/rutuns"
#define RUTUNSR_EXEC	"/usr/bin/rutunsr"

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP_FMT =
"Provides access to remote host using ssh.\n"
"%s\n"
"/[<user>@]<host>[:<port>][<options>]/... <args>\n"
"    Connect to service ... at <user>@<host>:<port> using ssh.\n"
"\n"
"    Options:\n"
"    ?controlpersist=<seconds>\n"
"        Set ControlPersist time in seconds. Default is 1.\n"
"    ?controltag=<tag>\n"
"        Used to generate a ControlPath. Required to set up control\n"
"        master functionality (if available).\n";

int			nsshargs = 0;
char			*sshargs[1024];

// preloaded configuration settings
char			*conf_ssh_controlpathfmt = NULL;
char			*conf_ssh_controlpersist = NULL;
char			*conf_tool_type = NULL;
char			*conf_tool_exec = NULL;

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
	struct passwd	*pw;

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
	char	*s2;
	char	*a, *b;

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


/**
* Read bytes from fd until delimiter found. All read bytes,
* including are consumed/dropped.
*
* Blocking read on fd only returns when something is available or EOF
* is reached (fd closed on other end).
*
* @param fd		fd to read from
* @param delim		char string to match
* @param delimsz	# of bytes in delim
* @return		0 on success; -1 on error
*/
int
consume_delimiter(int fd, char *delim, int delimsz) {
	char	buf[16384];
	int	bri, bwi, n;

	if (delimsz > sizeof(buf)) {
		return -1;
	}
	buf[0] = '\0';

	/* fill buffer to delimsz */
	for (bwi = 0; bwi < delimsz; ) {
		if ((n = russ_read(fd, &buf[bwi], delimsz-bwi)) <= 0) {
			return -1;
		}
		bwi += n;
	}

	/* compare buffer with delim, read byte at a time */
	for (bri = 0; ; ) {
		if (memcmp(&buf[bri], delim, delimsz) == 0) {
			return 0;
		}
		bri++;

		/* move delimsz-1 buf contents from end to start */
		if (bwi == sizeof(buf)) {
			memmove(&buf[bri], buf, delimsz-1);
			bri = 0;
			bwi = delimsz-1;
		}
		if (russ_read(fd, &buf[bwi], 1) <= 0) {
			return -1;
		}
		bwi++;
	}
	return -1;
}

/**
* Generate delimiter string. Composed of some one-time information:
* * timestamp
* * random sequence
*
* Note: there is no need to leak any local information such as uid.
*/
char *
generate_delimiter(void) {
	char	buf[1024];

	if (russ_snprintf(buf, sizeof(buf), "____SSHR_DELIM____%ld_%s",
		russ_gettime(), "1234") < 0) {
		return NULL;
	}
	return strdup(buf);
}

void
execute(struct russ_sess *sess, char *userhostport, char **opts, char *new_spath) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	struct russ_cconn	*cconn = NULL;
	char			*uhp_user = NULL, *uhp_host = NULL, *uhp_port = NULL, *uhp_opt = NULL;
	char			controlpathopt[1024], controlpersistopt[32], tmp[1024];
	char			russssh_dirpath[1024];
	int			i, status, pid;

	sconn = sess->sconn;
	req = sess->req;

	switch_user(sconn);

	/* parse out user, host, and port */
	if ((uhp_user = strdup(userhostport)) == NULL) {
		goto answer_exit_callfailure;
	}
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

	/* augment sshargs array */
	if ((opts) && (opts[0] != NULL)) {
		char	*controlpersist = NULL, *controltag = NULL;
		char	*user_home = NULL;

		controltag = russ_sarray0_get_suffix(opts, "controltag=");

		user_home = dup_user_home();

		if (controltag
			&& user_home
			&& (russ_snprintf(russssh_dirpath, sizeof(russssh_dirpath), "%s/.ssh/russssh", user_home) > 0)
			&& (ensure_mkdir(russssh_dirpath, 0700) == 0)
			&& (russ_snprintf(tmp, sizeof(tmp), "ControlPath=%s/%s", russssh_dirpath, conf_ssh_controlpathfmt) > 0)
			&& (russ_snprintf(controlpathopt, sizeof(controlpathopt), tmp, controltag) > 0)) {

			//fprintf(stderr, "controlpathopt (%s)\n", controlpathopt);

			sshargs[nsshargs++] = "-o";
			sshargs[nsshargs++] = "ControlMaster=auto";

			sshargs[nsshargs++] = "-o";
			sshargs[nsshargs++] = controlpathopt;

			controlpersist = russ_sarray0_get_suffix(opts, "controlpersist=");
			if (!controlpersist) {
				controlpersist = conf_ssh_controlpersist;
			}
			sshargs[nsshargs++] = "-o";
			if (russ_snprintf(controlpersistopt, sizeof(controlpersistopt), "ControlPersist=%s", controlpersist) > 0) {
				sshargs[nsshargs++] = controlpersistopt;
			} else {
				sshargs[nsshargs++] = "ControlPersist=1";
			}
		}
		user_home = russ_free(user_home);
	}
	if (uhp_user) {
		sshargs[nsshargs++] = "-l";
		sshargs[nsshargs++] = uhp_user;
	}
	if (uhp_port) {
		sshargs[nsshargs++] = "-p";
		sshargs[nsshargs++] = uhp_port;
	}
	sshargs[nsshargs++] = uhp_host;

	if ((strcmp(conf_tool_type, "tunnel") == 0)
		|| (strcmp(conf_tool_type, "tunnelr") == 0)) {
		int	cconn_send_rv;
		int	cfds[RUSS_CONN_NFDS], tmpfd;
		char	*delim = NULL;
		char	delimattr[1024];
		int	delimpos, delimsz;

		/* select tool exec */
		if (conf_tool_exec) {
			sshargs[nsshargs++] = conf_tool_exec;
		} else {
			if (strcmp(conf_tool_type, "tunnel") == 0) {
				sshargs[nsshargs++] = RUTUNS_EXEC;
			} else {
				sshargs[nsshargs++] = RUTUNSR_EXEC;
			}
		}
		sshargs[nsshargs++] = NULL;

		/* set up fds to pass to ssh server */
		russ_fds_init(cfds, RUSS_CONN_NFDS, -1);
		russ_fds_init(sconn->fds, RUSS_CONN_NFDS, -1);
		if (russ_make_pipes(RUSS_CONN_STD_NFDS, cfds, sconn->fds) < 0) {
			exit(0);
		}

		tmpfd = cfds[0];
		cfds[0] = sconn->fds[0];
		sconn->fds[0] = tmpfd;

		/* fix up fds and exec (COMMON) */
		signal(SIGCHLD, SIG_DFL);
		if ((pid = fork()) == 0) {
			/* dup sconn stdin/out/err fds to standard stdin/out/err */
			if ((dup2(sconn->fds[0], 0) >= 0) &&
				(dup2(sconn->fds[1], 1) >= 0) &&
				(dup2(sconn->fds[2], 2) >= 0)) {

				/* close fds */
				russ_fds_close(cfds, RUSS_CONN_NFDS);
				russ_sconn_close(sconn);

				execv(sshargs[0], sshargs);
			}

			/* should not get here! */
			russ_dprintf(2, "error: could not execute\n");
			exit(1);
		}

		/* close non-sysfds (comm with ssh is now over stdin/out/err) */
		russ_fds_close(sconn->fds, RUSS_CONN_NFDS);

		/* tunnel request */
		if ((cconn = russ_cconn_new()) == NULL) {
			goto answer_exit_callfailure;
		}

		/* remove old delimiter, if present */
		if (((delimpos = russ_sarray0_find_prefix(req->attrv, "__SSHR_DELIM__=")) >= 0)
			&& (russ_sarray0_remove(req->attrv, delimpos) < 0)) {
			goto answer_exit_callfailure;
		}

		/* augment request with delimiter attribute as appropriate */
		if (strcmp(conf_tool_type, "tunnelr") == 0) {
			if (((delim = generate_delimiter()) == NULL)
				|| ((delimsz = strlen(delim)) < 0)
				|| (russ_snprintf(delimattr, sizeof(delimattr), "__SSHR_DELIM__=%s", delim) < 0)
				|| (russ_sarray0_append(&req->attrv, delimattr, NULL) < 0)) {
				goto answer_exit_callfailure;
			}
		}

		/* send request over cfds[0] */
		cconn->sd = cfds[0];
		req->spath = new_spath; /* not saving old req->spath! */
		cconn_send_rv = russ_cconn_send_req(cconn, RUSS_DEADLINE_NEVER, req);
		cconn->sd = -1;

		if (delim) {
			/* wait for and consume delimiter on stdout and stderr in cfds */
			if ((consume_delimiter(cfds[1], delim, delimsz) < 0)
				|| (consume_delimiter(cfds[2], delim, delimsz) < 0)) {
				goto answer_exit_callfailure;
			}
		}

		/* answer client */
		if (russ_sconn_answer(sconn, RUSS_CONN_STD_NFDS, cfds) < 0) {
			/* can't return anything; not even exit status */
			exit(0);
		}
		//russ_fds_close(cfds, 3);
		if (cconn_send_rv < 0) {
			russ_sconn_exit(sconn, RUSS_EXIT_CALLFAILURE);
			exit(0);
		}
		//cconn = russ_cconn_free(cconn);
	} else {
		sshargs[nsshargs++] = conf_tool_exec ? conf_tool_exec : RUDIAL_EXEC;
		if ((req->attrv != NULL) && (req->attrv[0] != NULL)) {
			for (i = 0; req->attrv[i] != NULL; i++) {
				sshargs[nsshargs++] = "-a";
				if ((sshargs[nsshargs++] = escape_special(req->attrv[i])) == NULL) {
					russ_sconn_fatal(sconn, "error: out of memory", RUSS_EXIT_FAILURE);
					exit(0);
				}
			}
		}
		sshargs[nsshargs++] = req->op;
		sshargs[nsshargs++] = new_spath;
		if ((req->argv != NULL) && (req->argv[0] != NULL)) {
			for (i = 0; req->argv[i] != NULL; i++) {
				if ((sshargs[nsshargs++] = escape_special(req->argv[i])) == NULL) {
					russ_sconn_fatal(sconn, "error: out of memory", RUSS_EXIT_FAILURE);
					exit(0);
				}
			}
		}
		sshargs[nsshargs++] = NULL;

		/* fix up fds and exec (COMMON) */
		signal(SIGCHLD, SIG_DFL);
		if ((pid = fork()) == 0) {
			/* dup sconn stdin/out/err fds to standard stdin/out/err */
			if ((dup2(sconn->fds[0], 0) >= 0) &&
				(dup2(sconn->fds[1], 1) >= 0) &&
				(dup2(sconn->fds[2], 2) >= 0)) {

				/* close fds */
				russ_sconn_close(sconn);

				execv(sshargs[0], sshargs);
			}
			/* should not get here! */
			russ_dprintf(2, "error: could not execute\n");
			exit(1);
		}

		/* close non-sysfds (comm with ssh is now over stdin/out/err) */
		russ_fds_close(sconn->fds, RUSS_CONN_NFDS);

		/* autoanswer is set up for server in main() */
	}

	/* wait for exit value, pass back, and close up */
	if (__russ_waitpidfd(pid, &status, sconn->sysfds[0], 200) == __RUSS_WAITPIDFD_FD) {
		status = RUSS_EXIT_EXITFDCLOSED;
		kill(pid, SIGHUP);
	}
	russ_sconn_exit(sconn, WEXITSTATUS(status));
	russ_sconn_close(sconn);

	exit(0);

answer_exit_callfailure:
	/* setup sysfds[0] and return exit value */
	russ_sconn_answer(sconn, 0, NULL);
	russ_sconn_exit(sconn, RUSS_EXIT_CALLFAILURE);
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
	if ((p = strchr(userhost, '/')) == NULL) {
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

/**
* Handler for /<user@host:port>/...
*
* @param sess		session object
*/
void
svc_userhostport_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*newspath = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if (strcmp(req->spath, sess->spath) == 0) {
		/* no newspath */
		if (req->opnum == RUSS_OPNUM_LIST) {
			russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
			exit(0);
		}
	} else {
		if ((newspath = strchr(req->spath+1, '/')) == NULL) {
			return;
		}
		execute(sess, sess->name, sess->options, newspath);
	}
}

/**
* Set up sshargs with default/fixed settings and those from the
* configuration file.
*
* @return		0 on success; -1 on failure
*/
int
setup_sshargs(void) {
	char	**options = NULL, *option = NULL, *value = NULL;
	char	sshoption[1024];
	int	i;

	nsshargs = 0;
	sshargs[nsshargs++] = SSH_EXEC;
	sshargs[nsshargs++] = "-o";
	sshargs[nsshargs++] = "StrictHostKeyChecking=no";
	sshargs[nsshargs++] = "-o";
	sshargs[nsshargs++] = "BatchMode=yes";
	sshargs[nsshargs++] = "-o";
	sshargs[nsshargs++] = "LogLevel=QUIET";
	sshargs[nsshargs++] = "-o";
	sshargs[nsshargs++] = "ForwardX11=no";
	sshargs[nsshargs++] = "-o";
	sshargs[nsshargs++] = "ForwardX11Trusted=no";

	if ((options = russ_conf_options(conf, "options")) != NULL) {
		for (i = 0, option = options[i]; option != NULL; i++, option = options[i]) {
			value = russ_conf_get(conf, "options", option, NULL);
			if (russ_snprintf(sshoption, sizeof(sshoption), "%s=%s", option, value) < 0) {
				value = russ_free(value);
				options = russ_sarray0_free(options);
				return -1;
			}
			sshargs[nsshargs++] = "-o";
			sshargs[nsshargs++] = strdup(sshoption);
			value = russ_free(value);
		}
		options = russ_sarray0_free(options);
	}
	return 0;
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russssh2_server [<conf options>]\n"
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
	char			help[4096];
	int			autoanswer;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	conf_ssh_controlpathfmt = russ_conf_get(conf, "ssh", "controlpathfmt", "%s-%%C");
	conf_ssh_controlpersist = russ_conf_get(conf, "ssh", "controlpersist", "1");
	conf_tool_type = russ_conf_get(conf, "tool", "type", "dial");
	conf_tool_exec = russ_conf_get(conf, "tool", "exec", NULL);

	/* autoanswer and help settings */
	if (strcmp(conf_tool_type, "tunnelr") ==0) {
		russ_snprintf(help, sizeof(help), HELP_FMT, "\nRunning as a clean relay.\n");
		autoanswer = 0;
	} else if (strcmp(conf_tool_type, "tunnel") == 0) {
		russ_snprintf(help, sizeof(help), HELP_FMT, "");
		autoanswer = 0;
	} else {
		russ_snprintf(help, sizeof(help), HELP_FMT, "");
		autoanswer = 1;
	}

	if (setup_sshargs() < 0) {
		fprintf(stderr, "error: cannot setup sshargs\n");
		exit(1);
	}

	/* set up server and service tree */
	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_autoswitchuser(svr, 1) < 0)
		|| (russ_svr_set_matchclientuser(svr, 1) < 0)
		|| (russ_svr_set_allowrootuser(svr, 1) < 0)
		|| (russ_svr_set_help(svr, help) < 0)

		|| (russ_svcnode_set_handler(svr->root, svc_root_handler) < 0)

		|| ((node = russ_svcnode_add(svr->root, "*", svc_userhostport_handler)) == NULL)
		|| (russ_svcnode_set_autoanswer(node, autoanswer) < 0)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
	}
	russ_svr_loop(svr);
	exit(0);
}
