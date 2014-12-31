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

#define DEFAULT_STATUS "pid:ppid:pgrp:sid:uid:gid:state:comm:cmdline"

/* global */
struct russ_conf	*conf = NULL;
const char		*HELP = 
"Report on, monitor, and kill processes.\n"
"\n"
"/g/<gid>/status\n"
"    Return the status of processes owned by a group.\n"
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
"    Return the status of the process. If -l is specified, a\n"
"    key=value format is used; the full cmdline is also output.\n"
"\n"
"/p/<pid>/wait [<interval> [<timeout>]]\n"
"    Wait for the process to terminate. The status of the process is\n"
"    checked every interval milliseconds (default/minimum is 1000ms)\n"
"    for a maximum of timeout milliseconds (default is infinite).\n"
"\n"
"/u/<uid>/status\n"
"    Return the status of processes owned by a user.\n"
"\n"
"The default status format is:\n"
"    "DEFAULT_STATUS"\n";

/* process attributes info */
#define PATTR_NULL		0
#define PATTR_PID		1
#define PATTR_COMM		2
#define PATTR_CMDLINE		3
#define PATTR_STATE		4
#define PATTR_PPID		5
#define PATTR_PGRP		6
#define PATTR_SID		7
#define PATTR_TTY		8
#define PATTR_TPGID		9
#define PATTR_FLAGS		10
#define PATTR_MINFLT		11
#define PATTR_CMINFLT		12
#define PATTR_MAJFLT		13
#define PATTR_CMAJFLT		14
#define PATTR_UTIME		15
#define PATTR_STIME		16
#define PATTR_CUTIME		17
#define PATTR_CSTIME		18
#define PATTR_PRIORITY		19
#define PATTR_NICE		20
#define PATTR_NUMTHREADS	21
#define PATTR_ITREALVALUE	22
#define PATTR_STARTTIME		23
#define PATTR_VSIZE		24
#define PATTR_RSS		25
#define PATTR_RSSLIM		26
#define PATTR_STARTCODE		27
#define PATTR_ENDCODE		28
#define PATTR_STARTSTACK	29
#define PATTR_KSTKESP		30
#define PATTR_KSTKEIP		31
#define PATTR_SIGNAL		32
#define PATTR_BLOCKED		33
#define PATTR_SIGIGNORE		34
#define PATTR_SIGCATCH		35
#define PATTR_WCHAN		36
#define PATTR_NSWAP		37
#define PATTR_CNSWAP		38
#define PATTR_EXITSIGNAL	39
#define PATTR_PROCESSOR		40
#define PATTR_RTPRIORITY	41
#define PATTR_POLICY		42
#define PATTR_DELAYACCTBLKIOTICKS	43
#define PATTR_GUESTTIME		44
#define PATTR_CGUESTTIME	45
#define PATTR_UID		46
#define PATTR_GID		47

struct pattrs {
	int	value;
	char	*name;
};

struct pattrs pattrs[] = {
	{ PATTR_BLOCKED, "blocked" },
	{ PATTR_CGUESTTIME, "cguesttime" },
	{ PATTR_CMAJFLT, "cmajflt" },
	{ PATTR_CMDLINE, "cmdline" },
	{ PATTR_CMINFLT, "cminflt" },
	{ PATTR_CNSWAP, "cnswap" },
	{ PATTR_COMM, "comm" },
	{ PATTR_CSTIME, "cstime" },
	{ PATTR_CUTIME, "cutime" },
	{ PATTR_DELAYACCTBLKIOTICKS, "delayacctblkioticks" },
	{ PATTR_ENDCODE, "endcode" },
	{ PATTR_EXITSIGNAL, "exitsignal" },
	{ PATTR_FLAGS, "flags" },
	{ PATTR_GID, "gid" },
	{ PATTR_GUESTTIME, "guesttime" },
	{ PATTR_ITREALVALUE, "itrealvalue" },
	{ PATTR_KSTKEIP, "kstkeip" },
	{ PATTR_KSTKESP, "kstkesp" },
	{ PATTR_MAJFLT, "majflt" },
	{ PATTR_MINFLT, "minflt" },
	{ PATTR_NICE, "nice" },
	{ PATTR_NSWAP, "nswap" },
	{ PATTR_NUMTHREADS, "numthreads" },
	{ PATTR_PGRP, "pgrp" },
	{ PATTR_PID, "pid" },
	{ PATTR_POLICY, "policy" },
	{ PATTR_PPID, "ppid" },
	{ PATTR_PRIORITY, "priority" },
	{ PATTR_PROCESSOR, "processor" },
	{ PATTR_RSS, "rss" },
	{ PATTR_RSSLIM, "rsslim" },
	{ PATTR_RTPRIORITY, "rtpriority" },
	{ PATTR_SID, "sid" },
	{ PATTR_SIGCATCH, "sigcatch" },
	{ PATTR_SIGIGNORE, "sigignore" },
	{ PATTR_SIGNAL, "signal" },
	{ PATTR_STARTCODE, "startcode" },
	{ PATTR_STARTSTACK, "startstack" },
	{ PATTR_STARTTIME, "starttime" },
	{ PATTR_STATE, "state" },
	{ PATTR_STIME, "stime" },
	{ PATTR_TPGID, "tpgid" },
	{ PATTR_TTY, "tty" },
	{ PATTR_UID, "uid" },
	{ PATTR_UTIME, "utime" },
	{ PATTR_VSIZE, "vsize" },
	{ PATTR_WCHAN, "wchan" },
	{ PATTR_NULL, NULL },
};

#define PATTR_IDXS_SIZE 256
typedef int pattr_idxs[PATTR_IDXS_SIZE];

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
	int		sz;

	if ((snprintf(pid_path, sizeof(pid_path), "/proc/%d", pid) < 0)
		|| (snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid) < 0)) {
		return -1;
	}
	if (stat(pid_path, &st) < 0) {
		return -1;
	}
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

	pi->cmdline[0] = '\0';
	f = NULL;
	if ((snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid) < sizeof(cmdline_path))
		&& ((f = fopen(cmdline_path, "r")) != NULL)
		&& ((sz = fread(pi->cmdline, 1, sizeof(pi->cmdline), f)) >= 0)) {
		int	i;
		char	*ch;

		for (i = 0, ch = pi->cmdline; i < sz; i++, ch++) {
			if (*ch == '\0') {
				if (*(ch+1) == '\0') {
					/* see proc man page for why */
					break;
				}
				*ch = ' ';
			} else if ((*ch == '\n') || (*ch == '\r')) {
				*ch = ' ';
			}
		}
		pi->cmdline[sz] = '\0';
	}
	fclose(f);

	return 0;
}

int
get_pattr_idx(char *s) {
	int	i;

	for (i = 0; pattrs[i].value != PATTR_NULL; i++) {
		if (strcmp(pattrs[i].name, s) == 0) {
			break;
		}
	}
	return i;
}

int
parse_pattr_idxs(char *s, pattr_idxs pattr_idxs) {
	char	*ps, *pe;
	int	i;

	s = strdup(s);
	for (i = 0, ps = s, pe = s; *pe != '\0'; pe++) {
		if (*pe == ':') {
			*pe = '\0';
			pattr_idxs[i++] = get_pattr_idx(ps);
			ps = pe+1;
		}
	}
	if (ps != pe) {
		pattr_idxs[i++] = get_pattr_idx(ps);
	}
	pattr_idxs[i] = PATTR_NULL;
	free(s);
	return 0;
}

int
dprint_pid_info(int fd, struct pid_info *pi, pattr_idxs pattr_idxs, int long_format) {
	int	i, idx;

	for (i = 0, idx = pattr_idxs[i]; idx != PATTR_NULL; i++, idx = pattr_idxs[i]) {
		if ((i) && (!long_format)) {
			russ_dprintf(fd, ":");
		}

		switch (pattrs[idx].value) {
		case PATTR_PID:
			russ_dprintf(fd, (long_format ? "pid=%d\n": "%d"), pi->pid);
			break;
		case PATTR_COMM:
			russ_dprintf(fd, (long_format ? "comm=%s\n": "%s"), pi->comm);
			break;
		case PATTR_CMDLINE:
			russ_dprintf(fd, (long_format ? "cmdline=%s\n": "%s"), pi->cmdline);
			break;
		case PATTR_STATE:
			russ_dprintf(fd, (long_format ? "state=%c\n": "%c"), pi->state);
			break;
		case PATTR_PPID:
			russ_dprintf(fd, (long_format ? "ppid=%d\n": "%d"), pi->ppid);
			break;
		case PATTR_PGRP:
			russ_dprintf(fd, (long_format ? "pgrp=%d\n": "%d"), pi->pgrp);
			break;
		case PATTR_SID:
			russ_dprintf(fd, (long_format ? "sid=%d\n": "%d"), pi->sid);
			break;
#if 0
		case PATTR_TTY:
			russ_dprintf(fd, (long_format ? "tty=%d\n": "%d"), pi->pid);
			break;
		case PATTR_TPGID:
			russ_dprintf(fd, (long_format ? "tpgid=%d\n": "%d"), pi->pid);
			break;
		case PATTR_FLAGS:
			russ_dprintf(fd, (long_format ? "flags=%d\n": "%d"), pi->pid);
			break;
		case PATTR_MINFLT:
			russ_dprintf(fd, (long_format ? "minflt=%d\n": "%d"), pi->pid);
			break;
		case PATTR_CMINFLT:
			russ_dprintf(fd, (long_format ? "cminflt=%d\n": "%d"), pi->pid);
			break;
		case PATTR_MAJFLT:
			russ_dprintf(fd, (long_format ? "majflt=%d\n": "%d"), pi->pid);
			break;
		case PATTR_CMAJFLT:
			russ_dprintf(fd, (long_format ? "cmajflt=%d\n": "%d"), pi->pid);
			break;
#endif
		case PATTR_UTIME:
			russ_dprintf(fd, (long_format ? "utime=%lu\n": "%lu"), pi->utime);
			break;
		case PATTR_STIME:
			russ_dprintf(fd, (long_format ? "stime=%lu\n": "%lu"), pi->stime);
			break;
		case PATTR_CUTIME:
			russ_dprintf(fd, (long_format ? "cutime=%ld\n": "%ld"), pi->cutime);
			break;
		case PATTR_CSTIME:
			russ_dprintf(fd, (long_format ? "cstime=%ld\n": "%ld"), pi->cstime);
			break;
#if 0
		case PATTR_PRIORITY:
			russ_dprintf(fd, (long_format ? "priority=%d\n": "%d"), pi->pid);
			break;
		case PATTR_NICE:
			russ_dprintf(fd, (long_format ? "nice=%d\n": "%d"), pi->pid);
			break;
		case PATTR_NUMTHREADS:
			russ_dprintf(fd, (long_format ? "numthreads=%d\n": "%d"), pi->pid);
			break;
		case PATTR_ITREALVALUE:
			russ_dprintf(fd, (long_format ? "itrealvalue=%d\n": "%d"), pi->pid);
			break;
#endif
		case PATTR_STARTTIME:
			russ_dprintf(fd, (long_format ? "starttime=%llu\n": "%llu"), pi->starttime);
			break;
		case PATTR_VSIZE:
			russ_dprintf(fd, (long_format ? "vsize=%lu\n": "%d"), pi->vsize);
			break;
		case PATTR_RSS:
			russ_dprintf(fd, (long_format ? "rss=%ld\n": "%d"), pi->rss);
			break;
#if 0
		case PATTR_RSSLIM:
			russ_dprintf(fd, (long_format ? "rsslim=%d\n": "%d"), pi->pid);
			break;
		case PATTR_STARTCODE:
			russ_dprintf(fd, (long_format ? "startcode=%d\n": "%d"), pi->pid);
			break;
		case PATTR_ENDCODE:
			russ_dprintf(fd, (long_format ? "endcode=%d\n": "%d"), pi->pid);
			break;
		case PATTR_STARTSTACK:
			russ_dprintf(fd, (long_format ? "startstack=%d\n": "%d"), pi->pid);
			break;
		case PATTR_KSTKESP:
			russ_dprintf(fd, (long_format ? "kstkesp=%d\n": "%d"), pi->pid);
			break;
		case PATTR_KSTKEIP:
			russ_dprintf(fd, (long_format ? "kstkeip=%d\n": "%d"), pi->pid);
			break;
		case PATTR_SIGNAL:
			russ_dprintf(fd, (long_format ? "signal=%d\n": "%d"), pi->pid);
			break;
		case PATTR_BLOCKED:
			russ_dprintf(fd, (long_format ? "blocked=%d\n": "%d"), pi->pid);
			break;
		case PATTR_SIGIGNORE:
			russ_dprintf(fd, (long_format ? "sigignore=%d\n": "%d"), pi->pid);
			break;
		case PATTR_SIGCATCH:
			russ_dprintf(fd, (long_format ? "sigcatch=%d\n": "%d"), pi->pid);
			break;
		case PATTR_WCHAN:
			russ_dprintf(fd, (long_format ? "wchan=%d\n": "%d"), pi->pid);
			break;
		case PATTR_NSWAP:
			russ_dprintf(fd, (long_format ? "nswap=%d\n": "%d"), pi->pid);
			break;
		case PATTR_CNSWAP:
			russ_dprintf(fd, (long_format ? "cnswap=%d\n": "%d"), pi->pid);
			break;
		case PATTR_EXITSIGNAL:
			russ_dprintf(fd, (long_format ? "exitsignal=%d\n": "%d"), pi->pid);
			break;
		case PATTR_PROCESSOR:
			russ_dprintf(fd, (long_format ? "processor=%d\n": "%d"), pi->pid);
			break;
		case PATTR_RTPRIORITY:
			russ_dprintf(fd, (long_format ? "rtpriority=%d\n": "%d"), pi->pid);
			break;
		case PATTR_POLICY:
			russ_dprintf(fd, (long_format ? "policy=%d\n": "%d"), pi->pid);
			break;
		case PATTR_DELAYACCTBLKIOTICKS:
			russ_dprintf(fd, (long_format ? "delayacctblkioticks=%d\n": "%d"), pi->pid);
			break;
		case PATTR_GUESTTIME:
			russ_dprintf(fd, (long_format ? "guesttime=%d\n": "%d"), pi->pid);
			break;
		case PATTR_CGUESTTIME:
			russ_dprintf(fd, (long_format ? "cguesttime=%d\n": "%d"), pi->pid);
			break;
#endif
		case PATTR_UID:
			russ_dprintf(fd, (long_format ? "uid=%d\n": "%d"), pi->uid);
			break;
		case PATTR_GID:
			russ_dprintf(fd, (long_format ? "gid=%d\n": "%d"), pi->gid);
			break;
		}
	}
	if (!long_format) {
		russ_dprintf(fd, "\n");
	}
}

void
svc_root_handler(struct russ_sess *sess) {
	/* auto hanlding in svr */
}

void
svc_n_status_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	struct pid_info		pi;
	pid_t			pid;
	DIR			*dirp;
	struct dirent		*entry;
	pattr_idxs		pattr_idxs;
	int			use_long_format;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		parse_pattr_idxs(DEFAULT_STATUS, pattr_idxs);
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
			if (dprint_pid_info(sconn->fds[1], &pi, pattr_idxs, use_long_format) < 0) {
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
	pattr_idxs		pattr_idxs;
	int			use_long_format;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		parse_pattr_idxs(DEFAULT_STATUS, pattr_idxs);
		use_long_format = (req->argv) && (strcmp(req->argv[0], "-l") == 0);
		if ((sscanf(req->spath, "/p/%d", &pid) < 0)
			|| (get_pid_info(pid, &pi, use_long_format) < 0)) {
			russ_sconn_fatal(sconn, "error: invalid pid", RUSS_EXIT_FAILURE);
			exit(0);
		}
		dprint_pid_info(sconn->fds[1], &pi, pattr_idxs, use_long_format);
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
			russ_sconn_fatal(sconn, RUSS_MSG_BADARGS, RUSS_EXIT_FAILURE);
			exit(0);
		}
		poll_delay = (poll_delay < 1000) ? 1000 : poll_delay;
		if ((req->argv) && (req->argv[0] != NULL) && (sscanf(req->argv[1], "%d", &timeout) < 0)) {
			russ_sconn_fatal(sconn, RUSS_MSG_BADARGS, RUSS_EXIT_FAILURE);
			exit(0);
		}
		deadline = russ_to_deadline(timeout);

		if ((sscanf(req->spath, "/p/%d", &pid) < 0)
			|| (snprintf(pid_path, sizeof(pid_path), "/proc/%d", pid) < 0)
			|| (stat(pid_path, &st) < 0)) {
			russ_sconn_fatal(sconn, "error: invalid pid", RUSS_EXIT_FAILURE);
			exit(0);
		}
		pollfds[0].fd = sconn->sysfds[RUSS_CONN_SYSFD_EXIT]; /* exit */
		pollfds[0].events = POLLHUP;

		/* periodically check pid */
		while (russ_to_deadlinediff(deadline) > 0) {
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
svc_ug_handler_helper(struct russ_sess *sess, int ug) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	DIR			*dirp;
	struct dirent 		*entry;
	char			path[PATH_MAX];
	struct stat		st;

	if (req->opnum == RUSS_OPNUM_LIST) {
		if ((dirp = opendir("/proc")) == NULL) {
			russ_sconn_fatal(sconn, "error: could not get process list", RUSS_EXIT_FAILURE);
			exit(0);
		}
		for (entry = readdir(dirp); entry != NULL; entry = readdir(dirp)) {
			if (isdigit(entry->d_name[0])) {
				if ((snprintf(path, sizeof(path), "/proc/%s", entry->d_name) < PATH_MAX)
					&& (stat(path, &st) == 0)) {
					if (ug == 'u') {
						russ_dprintf(sconn->fds[1], "%d\n", st.st_uid);
					} else {
						russ_dprintf(sconn->fds[1], "%d\n", st.st_gid);
					}
				}
			}
		}
		closedir(dirp);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_ug_uidgid_status_handler_helper(struct russ_sess *sess, int ug) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	DIR			*dirp;
	struct dirent		*entry;
	char			path[PATH_MAX];
	pid_t			pid;
	struct pid_info		pi;
	char			*fmt;
	pattr_idxs		pattr_idxs;
	int			use_long_format;
	struct stat		st;
	uid_t			uid;
	gid_t			gid;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		parse_pattr_idxs(DEFAULT_STATUS, pattr_idxs);
		use_long_format = (req->argv) && (strcmp(req->argv[0], "-l") == 0);
		if (ug == 'u') {
			if (sscanf(req->spath, "/u/%d", &uid) < 0) {
				russ_sconn_fatal(sconn, "error: invalid uid", RUSS_EXIT_FAILURE);
				russ_sconn_close(sconn);
				exit(0);
			}
		} else {
			if (sscanf(req->spath, "/g/%d", &gid) < 0) {
				russ_sconn_fatal(sconn, "error: invalid gid", RUSS_EXIT_FAILURE);
				russ_sconn_close(sconn);
				exit(0);
			}
		}

		if ((dirp = opendir("/proc")) == NULL) {
			russ_sconn_fatal(sconn, "error: could not get process list", RUSS_EXIT_FAILURE);
			exit(0);
		}
		for (entry = readdir(dirp); entry != NULL; entry = readdir(dirp)) {
			if ((sscanf(entry->d_name, "%d", &pid) < 0)
				|| (snprintf(path, sizeof(path), "/proc/%s", entry->d_name) >= sizeof(path))
				|| (stat(path, &st) < 0)) {
				continue;
			}
			if (ug == 'u') {
				if (uid != st.st_uid) {
					continue;
				}
			} else if (ug == 'g') {
				if (gid != st.st_gid) {
					continue;
				}
			}
			if  (get_pid_info(pid, &pi, use_long_format) < 0) {
				continue;
			}
			if (dprint_pid_info(sconn->fds[1], &pi, pattr_idxs, use_long_format) < 0) {
				break;
			}
		}
		closedir(dirp);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_u_handler(struct russ_sess *sess) {
	svc_ug_handler_helper(sess, 'u');
}

void
svc_u_uid_status_handler(struct russ_sess *sess) {
	svc_ug_uidgid_status_handler_helper(sess, 'u');
}

void
svc_g_handler(struct russ_sess *sess) {
	svc_ug_handler_helper(sess, 'g');
}

void
svc_g_gid_status_handler(struct russ_sess *sess) {
	svc_ug_uidgid_status_handler_helper(sess, 'g');
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_proc [<conf options>]\n"
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
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
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
		|| ((node = russ_svcnode_add(root, "n", NULL)) == NULL)
		//|| ((node = russ_svcnode_add(node, "kill", svc_n_kill_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "status", svc_n_status_handler)) == NULL)

		|| ((node = russ_svcnode_add(root, "p", svc_p_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", NULL)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_add(node, "kill", svc_p_pid_kill_handler) == NULL)
		|| (russ_svcnode_add(node, "status", svc_p_pid_status_handler) == NULL)
		|| (russ_svcnode_add(node, "wait", svc_p_pid_wait_handler) == NULL)

		|| ((node = russ_svcnode_add(root, "u", svc_u_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", NULL)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_add(node, "status", svc_u_uid_status_handler) == NULL)

		|| ((node = russ_svcnode_add(root, "g", svc_g_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", NULL)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_add(node, "status", svc_g_gid_status_handler) == NULL)

		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK, RUSS_SVR_LIS_SD_DEFAULT)) == NULL)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
	}
	russ_svr_loop(svr);
	exit(0);
}
