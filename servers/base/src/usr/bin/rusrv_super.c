/*
** bin/rusrv_super.c
*/

/*
# license--start
#
# Copyright 2013 John Marshall
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

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#include <russ.h>

#define DEFAULT_DIAL_TIMEOUT	15000

/* global */
struct russ_conf	*conf = NULL;
char			*confdir = NULL, *trackdir = NULL;
char			*HELP = 
"Super server.\n";

/**
* Start server identified by server name.
*
* A lock file is used to prevent multiple concurrent attempts to
* start a server. The lock file _must_ be removed before exiting,
* regardless of success/failure.
*
* @param svrname	server name (used as lookup)
* @return		0 on success; -1 on failure
*/
int
start_server(char *svrname) {
	char	lockpath[PATH_MAX], pidpath[PATH_MAX], sockpath[PATH_MAX];
	char	*execfile = NULL, *conffile = NULL;
	char	serverpath_buf[PATH_MAX];
	int	pid, wpid;
	int	wst;

	// TODO: add checks to sprintf calls to prevent short printfs
	if ((sprintf(lockpath, "%s/.lock.%s", trackdir, svrname+1) < 0)
		|| (sprintf(pidpath, "%s/.pid.%s", trackdir, svrname+1) < 0)
		|| (sprintf(sockpath, "%s/%s", trackdir, svrname+1) < 0)
		|| (sprintf(serverpath_buf, "server:path=%s", sockpath) < 0)) {
		return -1;
	}
	if (((execfile = russ_conf_get(conf, svrname, "execfile", NULL)) == NULL)
		|| ((conffile = russ_conf_get(conf, svrname, "conffile", NULL)) == NULL)
		|| ((lock_file(lockpath, 100, 10000) < 0))) {		
		/* TODO: should svrname section be removed/disabled since it is invalid? */
		goto error;
	}
	/* double fork and exec new server */
	if ((pid = fork()) == 0) {
		setsid();
		signal(SIGHUP, SIG_IGN);
		if (fork() == 0) {
			put_pid(pidpath, getpid());
			execl(execfile, execfile, "-f", conffile, "-c", serverpath_buf, (char *)NULL);
			exit(1);
		}
		exit(0);
	}
	/* TODO: is there is race here between wait and server connect-able? */
	if ((wpid = waitpid(pid, &wst, 0)) != pid) {
		goto unlock_and_error;
	}
	if (unlock_file(lockpath) < 0) {
		/* unexpected */
	}
	free(execfile);
	free(conffile);
	return 0;

unlock_and_error:
	unlock_file(lockpath);
error:
	free(execfile);
	free(conffile);
	return -1;
}

/**
* Stop server identified Sby server name.
*
* @param svrname	server name
* @return		0 on success; -1 on failure
*/
int
stop_server(char *svrname) {
	char		pidpath[PATH_MAX], sockpath[PATH_MAX];
	int		n, pid;

	if (!russ_conf_has_section(conf, svrname)) {
		if (((n = snprintf(pidpath, sizeof(pidpath)-1, "%s/.pid.%s", trackdir, svrname+1)) < 0)
			|| (n >= sizeof(pidpath))) {
			return -1;
		}
		pid = get_pid(pidpath);
		if ((pid >= 0) && (kill(pid, 0) == 0)) {
			if (((n = snprintf(sockpath, sizeof(sockpath)-1, "%s%s", trackdir, svrname)) < 0)
				|| (n >= sizeof(sockpath))) {
				return -1;
			}
			remove(pidpath);
			remove(sockpath);
			kill(pid, SIGTERM);
			if (kill(pid, 0) < 0) {
				return -1;
			}
		}
	}
	return 0;
}

/**
* Simple case for russ_spath_resolve_with_uid without current uid.
*/
char *
_russ_spath_resolve(char *spath) {
	uid_t	uid;
	uid = getuid();
	return russ_spath_resolve_with_uid(spath, &uid, 0);
}

/**
* Set up announce symlinks to the super server.
*
* @return		number of symlinks created; -1 on failure
*/
int
setup_announce_paths(void) {
	char	*path = NULL, *superpath = NULL, *rpath = NULL;
	char	sympath[PATH_MAX];
	char	**sections, *section;
	int	cnt = 0, i, n;

	superpath = russ_conf_get(conf, "server", "path", NULL);
	if ((sections = russ_conf_sections(conf)) == NULL) {
		return -1;
	}

	for (i = 0, section = sections[i]; section != NULL; i++, section = sections[i]) {
		free(path);
		free(rpath);
		path = NULL;
		rpath = NULL;

		if ((section[0] != '/')
			|| ((path = russ_conf_get(conf, section, "path", NULL)) == NULL)
			|| ((rpath = _russ_spath_resolve(path)) == NULL)
			|| ((n = snprintf(sympath, sizeof(sympath), "%s%s", superpath, section)) < 0)
			|| (n >= sizeof(sympath))) {
			continue;
		}
		if ((unlink(rpath) < 0) || (symlink(sympath, rpath) < 0)) {
			continue;
		}
		cnt++;
	}
	free(superpath);
	free(path);
	free(rpath);
	russ_conf_sarray0_free(sections);
	return cnt;
}

/**
* Set up the trackdir (working directory).
*
* The trackdir is where super puts all the temporary socket,
* pid, and lock files.
*
* @return		0 on success; -1 on failure
*/
int
setup_trackdir(void) {
	if ((mkdir(trackdir, 0755) < 0) && (errno != EEXIST)) {
		return -1;
	}
	return 0;
}

/**
* Walk trackdir and clean up based on conf settings.
*
* For each entry in the trackdir, we check to see if it is
* defined in the super.conf file. If not, then we try to kill the
* server and clean up. Currently, this is only possible if the pid
* file of the running server is present and we kill(). However, this
* is not perfect since the pid is not guaranteed to belong to the
* server (for various reasons). Ideally, if we could simply remove
* the socket file and have the server process "find out" then the
* clean up would be trivial: remove().
*
* @return		0 on success; -1 on failure
*/
int
clean_trackdir(void) {
	DIR		*dirp;
	struct dirent	*dire;
	char		svrname[RUSS_REQ_SPATH_MAX];
	int		n;

	if ((dirp = opendir(trackdir)) == NULL) {
		return -1;
	}
	for (dire = readdir(dirp); dire != NULL; dire = readdir(dirp)) {
		if (dire->d_name[0] == '.') {
			/* ignore ., .., and hidden files */
			continue;
		}
		if (((n = snprintf(svrname, sizeof(svrname), "/%s", dire->d_name)) < 0)
			|| (n >= sizeof(svrname))
			|| (stop_server(svrname) < 0)) {
			// bufsize problem or failed; what to do?
			fprintf(stderr, "warning: failed to stop server (%s)\n", svrname);
		}
	}
	closedir(dirp);
	return 0;
}

/**
* Create a lock (using a file object).
*
* @param path		path to use for lock
* @param inter		retry interval
* @param timeout	maximum time (ms) to retry
* @return		0 on success; -1 on failure
*/
int
lock_file(char *path, int inter, int timeout) {
	for (; timeout > 0; timeout -= RUSS__MIN(timeout,inter)) {
		if (mkdir(path, 700) == 0) {
			return 0;
		}
		usleep(inter*1000);
	}
	return -1;
}

/**
* Remove lock (created with lock_file).
*
* @param path		path of the lock
* @return		0 on success; -1 on failure
*/
int
unlock_file(char *path) {
	return (rmdir(path) == 0) ? 0 : -1;
}

/**
* Get pid from file.
*
* @param path		file with pid
* @return		pid value; -1 on failure
*/
int
get_pid(char *path) {
	FILE	*f;
	int	pid;

	if (((f = fopen(path, "r")) == NULL)
		|| (fscanf(f, "%d", &pid) < 0)) {
		pid = -1;
	}
	if (f != NULL) {
		fclose(f);
	}
	return pid;
}

/**
* Store pid to a file.
*
* @param path		file for pid
* @return		0 on success; -1 on failure
*/
int
put_pid(char *path, int pid) {
	FILE	*f;

	if (((f = fopen(path, "w+")) == NULL)
		|| (fprintf(f, "%d", pid) < 0)) {
		pid = -1;
	}
	if (f != NULL) {
		fclose(f);
	}
	return (pid < 0) ? -1 : pid;
}

/**
* Match spath with server address being served by super.
*
* @param spath		request spath
* @return		server name (matched prefix of spath); NULL on failure
*/
char *
match_svrname(char *spath) {
	char	buf[RUSS_REQ_SPATH_MAX];
	char	*p;

	if (spath[0] == '\0') {
		return NULL;
	}
	strncpy(buf, spath, sizeof(buf)-1);
	buf[RUSS_REQ_SPATH_MAX-1] = '\0';
	p = strchr(buf+1, '/');
	while (1) {
		if (p != NULL) {
			*p = '\0';
		}
		if (russ_conf_has_section(conf, buf)) {
			return strdup(buf);
		}
		if (p == NULL) {
			break;
		}
		*p = '/';
		p = strchr(p+1, '/');
	}
	return NULL;
}

char *
russ_spath_reprefix(char *spath, char *oldpref, char *newpref) {
	char	buf[RUSS_REQ_SPATH_MAX];
	int	n;

	if (((n = snprintf(buf, sizeof(buf), "%s/%s", newpref, &spath[strlen(oldpref)])) < 0)
		|| (n >= sizeof(buf))) {
		return NULL;
	}
	return strdup(buf);
}

/**
* Dial a service and splice its client connection fds into the given
* server connection fds. The effective uid/gid are set prior to the
* call and restored before returning.
*
* Security: if the set[e]uid and set[e]gid calls fail on setup or
* restore, then the server exits immediately.
*
* @param sess		session object
* @return		0 on success; -1 on failure
*/
int
redial_and_splice(struct russ_sess *sess, char *svrname) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;	
	struct russ_cconn	*cconn;
	int			cnt;

	for (cnt = 3; cnt > 0; cnt--) {
		/* switch user */
		if ((setegid(sconn->creds.gid) < 0)
			|| (seteuid(sconn->creds.uid) < 0)) {
			/* this should not happen for root */
			russ_standard_answer_handler(sconn);
			/* TODO: what msg should be output? */
			russ_sconn_fatal(sconn, RUSS_MSG_NO_SWITCH_USER, RUSS_EXIT_FAILURE);
			exit(0);
		}

		/* connect as request user */
		/* TODO: what timeout should be used? */
		cconn = russ_dialv(russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req->op, req->spath, req->attrv, req->argv);

		/* switch (back) user */
		if ((seteuid(getuid()) < 0)
			|| (setegid(getgid()) < 0)) {
			russ_standard_answer_handler(sconn);
			/* TODO: what msg should be output? */
			russ_sconn_fatal(sconn, RUSS_MSG_NO_SWITCH_USER, RUSS_EXIT_FAILURE);
			exit(0);
		}

		if (cconn == NULL) {
			start_server(svrname);
			usleep(500000);
		} else {
			if (russ_sconn_splice(sconn, cconn) < 0) {
				russ_cconn_close(cconn);
				return -1;
			}
			break;
		}
	}
	return (cnt == 0) ? -1 : 0;
}

/**
* Output list of server names.
*
* Supports flat server name list.
*
* @param sess		session object
* @return		0 on success; -1 on failure
*/
int
list_servers(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	char			*svrname, **svrnames;
	int			i;

	if ((svrnames = russ_conf_sections(conf)) == NULL) {
		return -1;
	}
	for (i = 0; ; i++) {
		if ((svrname = svrnames[i]) == NULL) {
			break;
		} else if (svrname[0] == '/') {
			russ_dprintf(sconn->fds[1], "%s\n", svrname+1);
		}
	}
	russ_conf_sarray0_free(svrnames);
	return 0;
}

/**
* Handler for the / service.
*
* @param sess		session object
*/
void
svc_root_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		list_servers(sess);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_server_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*svrname = NULL, *spath;
	char			buf[RUSS_REQ_SPATH_MAX];
	int			n;

	if ((svrname = match_svrname(req->spath)) == NULL) {
		goto no_service;
	}
	if (((n = snprintf(buf, sizeof(buf), "%s%s", trackdir, req->spath)) < 0)
		|| (n >= sizeof(buf))
		|| ((spath = strdup(buf)) == NULL)) {
		goto no_service;
	}
	free(req->spath);
	req->spath = spath;
	if (redial_and_splice(sess, svrname) < 0) {
		goto no_service;
	}

no_service:
	russ_standard_answer_handler(sconn);
	russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
done:
	free(svrname);
	russ_sconn_close(sconn);
	exit(0);
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_super [<conf options>]\n"
"\n"
"Super server to intercept and route connections to servers.\n"
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
		|| ((node = russ_svcnode_add(root, "*", svc_server_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}

	if (((confdir = russ_conf_get(conf, "paths", "confdir", NULL)) == NULL)
		|| ((trackdir = russ_conf_get(conf, "paths", "trackdir", NULL)) == NULL)) {
		fprintf(stderr, "error: confdir or trackdir are not configured\n");
		exit(1);
	}
	if (setup_trackdir() < 0) {
		fprintf(stderr, "error: cannot set up trackdir (%s)\n", trackdir);
		exit(1);
	}
	if (clean_trackdir() < 0) {
		fprintf(stderr, "error: cannot clean trackdir (%s)\n", trackdir);
		exit(1);
	}
	if (setup_announce_paths() < 0) {
		fprintf(stderr, "error: cannot create symlinks\n");
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
	russ_svr_loop(svr);
	exit(0);	
}
