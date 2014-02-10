/*
** bin/rusrv_proc.c
*/

/*
# license--start
#
# Copyright 2012-2013 John Marshall
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>

extern char **environ;

#include <russ.h>

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Report on, monitor, and kill processes.\n"
"\n"
"/n/kill [<signal>] <pid> [...]\n"
"    The send the signal to the pids listed.\n"
"\n"
"/n/status <pid> [...]\n"
"    Return the status of the pids listed. See /p/<pid>/status for\n"
"    more.\n"
"\n"
"/p/<pid>/kill [<signal>]\n"
"    Send the signal to the process. The default signal is TERM.\n"
"    If the pid is negative, all processes in the process group are\n"
"    targetted.\n"
"\n"
"/p/<pid>/status [-l]\n"
"    Return the status of the process. The format is:\n"
"        pid:ppid:pgrp:sid:state:comm\n"
"    If -l is specified, a key=value format is used; the full\n"
"    cmdline is also output.\n"
"\n"
"/p/<pid>/wait [<interval> [<timeout>]]\n"
"    Wait for the process to terminate. The status of the process is\n"
"    checked every interval milliseconds (default/minimum is 1000ms)\n"
"    for a maximum of timeout milliseconds (default is infinite).\n";

void
svc_root_handler(struct russ_sess *sess) {
	/* auto hanlding in svr */
}

/* linux */
struct pid_info {
	pid_t		pid;
	char		comm[256];
	char		cmdline[8192];
	char		state;
	pid_t		ppid;
	pid_t		pgrp;
	pid_t		sid;
	/* tty */
	/* tpgid */
	/* flags */
	/* minflt */
	/* cminflt */
	/* majflt */
	/* cmajflt */
	unsigned long	utime;
	unsigned long	stime;
	long		cutime;
	long		cstime;
	/* priority */
	/* nice */
	/* num threads */
	/* itrealvalue */
	unsigned long long	starttime;
	unsigned long	vsize;
	long		rss;
	/* rsslim */
	/* startcode */
	/* endcode */
	/* startstack */
	/* kstkesp */
	/* kstkeip */
	/* signal */
	/* blocked */
	/* sigignore */
	/* sigcatch */
	/* wchan */
	/* nswap */
	/* cnswap */
	/* exit signal */
	/* processor */
	/* rt priority */
	/* policy */
	/* delayacct blkio ticks */
	/* guest time */
	/* cguest time */

	uid_t		uid;
	gid_t		gid;
};
#define PID_STAT_FORMAT "%d %255[^ ] %c %d %d %d %*s %*s %*s %*s %*s %*s %*s %lu %lu %ld %ld %*s %*s %*s %*s %llu %lu %ld"

typedef	int (*get_pid_info_fn)(pid_t, struct pid_info *, int);
get_pid_info_fn	get_pid_info = NULL;

int
linux_get_pid_info(pid_t pid, struct pid_info *pi, int use_long_format) {
	FILE		*f;
	struct stat	st;
	char		pid_path[1024], stat_path[1024], cmdline_path[1024];
	char		*p0, *p1;

	if ((snprintf(pid_path, sizeof(pid_path), "/proc/%d", pid) < 0)
		|| (snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid) < 0)) {
		return -1;
	}
	if (stat(pid_path, &st) < 0) {
		return -1;
	}
	pi->cmdline[0] = '\0';
	pi->uid = st.st_uid;
	pi->gid = st.st_gid;
	if (((f = fopen(stat_path, "r")) == NULL)
		|| (fscanf(f, PID_STAT_FORMAT, &(pi->pid), pi->comm, &(pi->state),
		&(pi->ppid), &(pi->pgrp), &(pi->sid), &(pi->utime),
		&(pi->stime), &(pi->cutime), &(pi->cstime), &(pi->starttime),
		&(pi->vsize), &(pi->rss)) < 0)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	if (use_long_format) {
		if ((snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid) > 0)
			&& ((f = fopen(cmdline_path, "r")) != NULL)) {
			char	*ch;

			fread(pi->cmdline, sizeof(pi->cmdline)-1, 1, f);
			pi->cmdline[sizeof(pi->cmdline)-1] = '\0';
			for (ch = pi->cmdline; ; ch++) {
				if (*ch == '\0') {
					*ch = ' ';
					if (*(ch+1) == '\0') {
						break;
					}
				} else if (*ch == '\n') {
					*ch = ' ';
				}
			}
			fclose(f);
		}
	}
	return 0;
}

int
dprint_pid_info(int fd, struct pid_info *pi, int use_long_format) {
	char	*fmt;

	if (use_long_format) {
		fmt = "pid=%d\nppid=%d\npgrp=%d\nsid=%d\nuid=%d\ngid=%d\nstate=%c\ncomm=%s\ncmdline=%s\n";
	} else {
		fmt = "%d:%d:%d:%d:%d:%d:%c:%s\n";
	}
	return russ_dprintf(fd, fmt,
		pi->pid, pi->ppid, pi->pgrp, pi->sid, pi->uid, pi->gid, pi->state, pi->comm, pi->cmdline);
}
 
void
svc_n_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	;
}

void
svc_n_status_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	struct pid_info		pi;
	pid_t			pid;
	DIR			*dirp;
	struct dirent		*entry;
	int			use_long_format;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		use_long_format = (req->argv) && (strcmp(req->argv[0], "-l") == 0);
		if ((dirp = opendir("/proc")) == NULL) {
			russ_sconn_fatal(sconn, "error: could not get process list", RUSS_EXIT_FAILURE);
			exit(0);
		}
		for (entry = readdir(dirp); entry != NULL; entry = readdir(dirp)) {
			if ((sscanf(entry->d_name, "%d", &pid) < 0)
				|| (get_pid_info(pid, &pi, use_long_format) < 0)) {
				continue;
			}
			if (dprint_pid_info(sconn->fds[1], &pi, use_long_format) < 0) {
				break;
			}
		}
		closedir(dirp);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_p_pid_kill_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	pid_t			pid;
	int			signum = -SIGTERM;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		if (sscanf(req->spath, "/p/%d", &pid) < 0) {
			russ_sconn_fatal(sconn, "error: invalid pid", RUSS_EXIT_FAILURE);
			exit(0);
		}
		if ((req->argv != NULL)
			&& (sscanf(req->argv[0], "%d", &signum) < 0)) {
			russ_sconn_fatal(sconn, "error: invalid signal", RUSS_EXIT_FAILURE);
		}
		if (kill(pid, -signum) < 0) {
			russ_sconn_fatal(sconn, "error: kill failed", RUSS_EXIT_FAILURE);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_p_pid_status_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	pid_t			pid;
	struct pid_info		pi;
	char			*fmt;
	int			use_long_format;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		use_long_format = (req->argv) && (strcmp(req->argv[0], "-l") == 0);
		if ((sscanf(req->spath, "/p/%d", &pid) < 0)
			|| (get_pid_info(pid, &pi, use_long_format) < 0)) {
			russ_sconn_fatal(sconn, "error: invalid pid", RUSS_EXIT_FAILURE);
			exit(0);
		}
		dprint_pid_info(sconn->fds[1], &pi, use_long_format);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_p_pid_wait_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	pid_t			pid;
	struct stat		st, st_last;
	struct pollfd		pollfds[1];
	russ_deadline		deadline;
	char			pid_path[1024];
	int			timeout;
	int			poll_delay;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		poll_delay = 1000;
		deadline = RUSS_DEADLINE_NEVER;

		if ((req->argv)	&& (sscanf(req->argv[0], "%d", &poll_delay) < 0)) {
			russ_sconn_fatal(sconn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
			exit(0);
		}
		poll_delay = (poll_delay < 1000) ? 1000 : poll_delay;
		if ((req->argv) && (req->argv[0] != NULL) && (sscanf(req->argv[1], "%d", &timeout) < 0)) {
			russ_sconn_fatal(sconn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
			exit(0);
		}
		deadline = russ_to_deadline(timeout);

		if ((sscanf(req->spath, "/p/%d", &pid) < 0)
			|| (snprintf(pid_path, sizeof(pid_path), "/proc/%d", pid) < 0)
			|| (stat(pid_path, &st) < 0)) {
			russ_sconn_fatal(sconn, "error: invalid pid", RUSS_EXIT_FAILURE);
			exit(0);
		}
		pollfds[0].fd = sconn->fds[3]; /* exit */
		pollfds[0].events = POLLHUP;

		/* periodically check pid */
		while (russ_to_deadline_diff(deadline) > 0) {
			switch (poll(pollfds, 1, poll_delay)) {
			case -1:
				if (errno != EINTR) {
					russ_sconn_fatal(sconn, "error: wait failed", RUSS_EXIT_FAILURE);
					exit(0);
				}
			case 0:
				/* timeout */
				if ((stat(pid_path, &st_last) < 0)
					|| (st.st_uid != st_last.st_uid)
					|| (st.st_gid != st_last.st_gid)) {
					russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
					exit(0);
				}
				break;
			default:
				/* exit fd closed (by other side) */
				russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
				exit(0);
			}
		}
		/* exceeded deadline */
		russ_sconn_exit(sconn, 2);
		exit(0);
	}
}

void
svc_p_pid_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	;
}

void
svc_p_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	DIR			*dirp;
	struct dirent 		*entry;

	if (req->opnum == RUSS_OPNUM_LIST) {
		if ((dirp = opendir("/proc")) == NULL) {
			russ_sconn_fatal(sconn, "error: could not get process list", RUSS_EXIT_FAILURE);
			exit(0);
		}
		for (entry = readdir(dirp); entry != NULL; entry = readdir(dirp)) {
			if (isdigit(entry->d_name[0])) {
				russ_dprintf(sconn->fds[1], "%s\n", entry->d_name);
			}
		}
		closedir(dirp);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_proc [<conf options>] [-- <hostsfile>]\n"
"\n"
"russ-based server for reporting on and monitoring processes.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*root, *node;
	struct russ_svr		*svr;
	struct utsname		utsname;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (uname(&utsname) < 0) {
		fprintf(stderr, "error: cannot determine system information\n");
		exit(1);
	}
	if (strcmp(utsname.sysname, "Linux") == 0) {
		get_pid_info = linux_get_pid_info;
	} else {
		fprintf(stderr, "error: no support for sysname\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "n", svc_n_handler)) == NULL)
		//|| ((node = russ_svcnode_add(node, "kill", svc_n_kill_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "status", svc_n_status_handler)) == NULL)

		|| ((node = russ_svcnode_add(root, "p", svc_p_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_p_pid_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((russ_svcnode_add(node, "kill", svc_p_pid_kill_handler)) == NULL)
		|| ((russ_svcnode_add(node, "status", svc_p_pid_status_handler)) == NULL)
		|| ((russ_svcnode_add(node, "wait", svc_p_pid_wait_handler)) == NULL)

		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up\n");
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
