/*
** bin/rusrv_debug.c
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
"/chargen\n"
"    Character generator outputting to stdout; follows the RPC 864\n"
"    the RFC 864 protocol sequence.\n"
"\n"
"/conn\n"
"    Outputs russ connection information.\n"
"\n"
"/daytime\n"
"    Outputs the date and time to the stdout.\n"
"\n"
"/discard [--perf]\n"
"    Discards all data received from stdin; if --perf is specified,\n"
"    performance feedback is provide to stderr, otherwise there is\n"
"    none.\n"
"\n"
"/echo\n"
"    Simple echo service; receives from stdin and outputs to stdout.\n"
"\n"
"/env\n"
"    Outputs environ entries to stdout.\n"
"\n"
"/request\n"
"    Outputs the request information at the server stdout.\n";

void
svc_root_handler(struct russ_conn *conn) {
	if (conn->req.op == RUSS_OP_HELP) {
		russ_dprintf(conn->fds[1], HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
}

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
		conn->creds.uid, conn->creds.gid, conn->creds.pid);
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
	struct russ_req	*req;
	int		fd;
	int		i;

	req = &(conn->req);
	fd = conn->fds[1];

	russ_dprintf(fd, "protocol string (%s)\n", req->protocol_string);
	russ_dprintf(fd, "spath (%s)\n", req->spath);
	russ_dprintf(fd, "op (%u)\n", req->op);

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
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_debug [<conf options>]\n"
"\n"
"russ-based server to aid in debugging russ commands.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svc_node	*root;
	struct russ_svr		*svr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((root = russ_svc_node_new("", svc_root_handler)) == NULL)
		|| (russ_svc_node_add(root, "chargen", svc_chargen_handler) == NULL)
		|| (russ_svc_node_add(root, "conn", svc_conn_handler) == NULL)
		|| (russ_svc_node_add(root, "daytime", svc_daytime_handler) == NULL)
		|| (russ_svc_node_add(root, "discard", svc_discard_handler) == NULL)
		|| (russ_svc_node_add(root, "echo", svc_echo_handler) == NULL)
		|| (russ_svc_node_add(root, "env", svc_env_handler) == NULL)
		|| (russ_svc_node_add(root, "request", svc_request_handler) == NULL)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}

	if (russ_svr_announce(svr,
		russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0666),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
