/*
** bin/rusrv_debug.c
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
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

extern char **environ;

#include "russ_conf.h"
#include "russ.h"

struct russ_conf	*conf = NULL;
char	*HELP = 
"Provides services useful for debugging. Unless otherwise stated,\n"
"stdin, stdout, and stderr all refer to the file descriptor triple\n"
"that is returned from a russ_dial call.\n"
"\n"
"chargen\n"
"    Character generator outputting to stdout; follows the RPC 864\n"
"    the RFC 864 protocol sequence.\n"
"\n"
"conn\n"
"    Outputs russ connection information.\n"
"\n"
"daytime\n"
"    Outputs the date and time to the stdout.\n"
"\n"
"discard [--perf]\n"
"    Discards all data received from stdin; if --perf is specified,\n"
"    performance feedback is provide to stderr, otherwise there is\n"
"    none.\n"
"\n"
"echo\n"
"    Simple echo service; receives from stdin and outputs to stdout.\n"
"\n"
"env\n"
"    Outputs environ entries to stdout.\n"
"\n"
"request\n"
"    Outputs the request information at the server stdout.\n";

void
svc_chargen_handler(struct russ_conn *conn) {
	char	buf[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ";
	char	off;

	off = 0;
	while (russ_dprintf(conn->fds[1], "%.72s\n", &(buf[off])) > 0) {
		off++;
		if (off > 94) {
			off = 0;
		}
		usleep(100000);
	}
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
svc_conn_handler(struct russ_conn *conn) {
	russ_dprintf(conn->fds[1], "uid (%d)\ngid (%d)\npid (%d)\n",
		conn->cred.uid, conn->cred.gid, conn->cred.pid);
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
svc_daytime_handler(struct russ_conn *conn) {
	char		buf[1024];
	time_t		now;
	struct tm	*now_tm;

	now = time(NULL);
	now_tm = localtime(&now);
	strftime(buf, sizeof(buf), "%A, %B %d, %Y %T-%Z", now_tm);
	russ_dprintf(conn->fds[1], "%s\n", buf);
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

static void
print_discard_stats(int fd, float elapsed, long total) {
	float	total_mb;

	total_mb = (float)total/(1<<20);
	russ_dprintf(fd, "total (MB): %.4f  elapsed (s): %.4f  throughput (MB/s): %.4f\n",
		total_mb, elapsed, total_mb/elapsed);
}

static double
gettimeofday_float(void) {
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	return ((double)tv.tv_sec)+((double)tv.tv_usec)/1000000.0;
}

void
svc_discard_handler(struct russ_conn *conn) {
	struct timeval	tv;
	double		t0, t1, last_t1;
	char		*buf;
	int		buf_size;
	long		n, total;
	int		perf = 0;

	/* 8MB */
	buf_size = 1<<23;
	if ((buf = malloc(buf_size)) == NULL) {
		russ_conn_fatal(conn, "error: cannot allocate buffer", RUSS_EXIT_FAILURE);
		return;
	}
	if ((russ_sarray0_count(conn->req.argv, 2) >= 1)
		&& (strcmp(conn->req.argv[0], "--perf") == 0)) {
		perf = 1;
	}
	t0 = gettimeofday_float();
	last_t1 = t0;
	total = 0;
	while (1) {
		n = russ_read(conn->fds[0], buf, buf_size);
		if (n > 0) {
			if (perf) {
				total += n;
				t1 = gettimeofday_float();
				if (t1-last_t1 > 2) {
					print_discard_stats(conn->fds[1], t1-t0, total);
					last_t1 = t1;
				}
			}
		} else if (n < 0) {
			if ((errno != EAGAIN) && (errno != EINTR)) {
				/* error */
				russ_conn_exit(conn, RUSS_EXIT_FAILURE);
				break;
			}
		} else {
			/* done */
			break;
		}
	}
	if (perf) {
		print_discard_stats(conn->fds[1], gettimeofday_float()-t0, total);
	}
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
svc_echo_handler(struct russ_conn *conn) {
	char	buf[1024];
	ssize_t	n;

	while ((n = russ_read(conn->fds[0], buf, sizeof(buf))) > 0) {
		russ_writen(conn->fds[1], buf, n);
	}
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
svc_env_handler(struct russ_conn *conn) {
	int	i;

	for (i = 0; environ[i] != NULL; i++) {
		russ_dprintf(conn->fds[1], "%s\n", environ[i]);
	}
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
svc_request_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	int			fd;
	int			i;

	req = &(conn->req);
	fd = conn->fds[1];

	russ_dprintf(fd, "protocol string (%s)\n", req->protocol_string);
	russ_dprintf(fd, "spath (%s)\n", req->spath);
	russ_dprintf(fd, "op (%s)\n", req->op);

	/* attrv */
	if (req->attrv == NULL) {
		russ_dprintf(fd, "attrv (NULL)\n");
	} else {
		for (i = 0; req->attrv[i] != NULL; i++) {
			russ_dprintf(fd, "attrv[%d] (%s)\n", i, req->attrv[i]);
		}
	}

	/* argv */
	if (req->argv == NULL) {
		russ_dprintf(fd, "argv (NULL)\n");
	} else {
		for (i = 0; req->argv[i] != NULL; i++) {
			russ_dprintf(fd, "argv[%d] (%s)\n", i, req->argv[i]);
		}
	}
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

void
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;

	req = &(conn->req);
	if (strcmp(req->op, "execute") == 0) {
		if (strcmp(req->spath, "/chargen") == 0) {
			svc_chargen_handler(conn);
		} else if (strcmp(req->spath, "/conn") == 0) {
			svc_conn_handler(conn);
		} else if (strcmp(req->spath, "/daytime") == 0) {
			svc_daytime_handler(conn);
		} else if (strcmp(req->spath, "/discard") == 0) {
			svc_discard_handler(conn);
		} else if (strcmp(req->spath, "/echo") == 0) {
			svc_echo_handler(conn);
		} else if (strcmp(req->spath, "/env") == 0) {
			svc_env_handler(conn);
		} else if (strcmp(req->spath, "/request") == 0) {
			svc_request_handler(conn);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else if (strcmp(req->op, "help") == 0) {
        	russ_dprintf(conn->fds[1], "%s", HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else if (strcmp(req->op, "list") == 0) {
		if (strcmp(req->spath, "/") == 0) {
			russ_dprintf(conn->fds[1], "chargen\nconn\ndaytime\ndiscard\necho\nenv\nrequest\n");
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_debug [<conf options>]\n"
"\n"
"russ-based server to aid in debugging russ commands.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	if ((argc < 2) || ((conf = russ_conf_init(&argc, argv, print_usage)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(-1);
	}

	lis = russ_announce(russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0666),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid()));
	if (lis == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_listener_loop(lis, master_handler);
	exit(0);
}
