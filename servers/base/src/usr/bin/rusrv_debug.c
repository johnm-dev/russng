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

#include <russ.h>

struct russ_conf	*conf = NULL;
const char		*HELP = 
"Provides services useful for debugging. Unless otherwise stated,\n"
"stdin, stdout, and stderr all refer to the file descriptor triple\n"
"that is returned from a russ_dial call.\n"
"\n"
"/chargen\n"
"    Character generator outputting to stdout; follows the RFC 864\n"
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
"    Outputs the request information at the server stdout.\n"
"\n"
"/whoami\n"
"    Outputs uid/gid and euid/egid information of running server\n"
"    (after user switch).\n";

void
svc_root_handler(struct russ_sess *sess) {
	/* auto handling by svr */
}

void
svc_chargen_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			buf[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ";
	char			off;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		off = 0;
		while (russ_dprintf(sconn->fds[1], "%.72s\n", &(buf[off])) > 0) {
			off++;
			if (off > 94) {
				off = 0;
			}
			usleep(100000);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_conn_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		russ_dprintf(sconn->fds[1], "uid (%d)\ngid (%d)\npid (%d)\n",
			sconn->creds.uid, sconn->creds.gid, sconn->creds.pid);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_daytime_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			buf[1024];
	time_t			now;
	struct tm		*now_tm;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		now = time(NULL);
		now_tm = localtime(&now);
		strftime(buf, sizeof(buf), "%A, %B %d, %Y %T-%Z", now_tm);
		russ_dprintf(sconn->fds[1], "%s\n", buf);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
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
svc_discard_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	struct timeval		tv;
	double			t0, t1, last_t1;
	char			*buf;
	int			buf_size;
	long			n, total;
	int			perf = 0;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		/* 8MB */
		buf_size = 1<<23;
		if ((buf = malloc(buf_size)) == NULL) {
			russ_sconn_fatal(sconn, "error: cannot allocate buffer", RUSS_EXIT_FAILURE);
			exit(0);
		}
		if ((russ_sarray0_count(sess->req->argv, 2) >= 1)
			&& (strcmp(sess->req->argv[0], "--perf") == 0)) {
			perf = 1;
		}
		t0 = gettimeofday_float();
		last_t1 = t0;
		total = 0;
		while (1) {
			n = russ_read(sconn->fds[0], buf, buf_size);
			if (n > 0) {
				if (perf) {
					total += n;
					t1 = gettimeofday_float();
					if (t1-last_t1 > 2) {
						print_discard_stats(sconn->fds[1], t1-t0, total);
						last_t1 = t1;
					}
				}
			} else if (n < 0) {
				if ((errno != EAGAIN) && (errno != EINTR)) {
					/* error */
					russ_sconn_exit(sconn, RUSS_EXIT_FAILURE);
					exit(0);
				}
			} else {
				/* done */
				break;
			}
		}
		if (perf) {
			print_discard_stats(sconn->fds[1], gettimeofday_float()-t0, total);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_echo_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			buf[1024];
	ssize_t			n;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		while ((n = russ_read(sconn->fds[0], buf, sizeof(buf))) > 0) {
			russ_writen(sconn->fds[1], buf, n);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_env_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	int			i;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		for (i = 0; environ[i] != NULL; i++) {
			russ_dprintf(sconn->fds[1], "%s\n", environ[i]);
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_request_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	int			fd;
	int			i;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		fd = sconn->fds[1];

		russ_dprintf(fd, "protocol string (%s)\n", req->protocol_string);
		russ_dprintf(fd, "spath (%s)\n", req->spath);
		russ_dprintf(fd, "op (%s)\n", req->op);
		russ_dprintf(fd, "opnum (%u)\n", req->opnum);

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
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_whoami_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		russ_dprintf(sconn->fds[1], "uid (%d) gid (%d)\n", getuid(), getgid());
		russ_dprintf(sconn->fds[1], "euid (%d) egid (%d)\n", geteuid(), getegid());
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
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
	struct russ_svcnode	*root;
	struct russ_svr		*svr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_init(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| (russ_svcnode_add(root, "chargen", svc_chargen_handler) == NULL)
		|| (russ_svcnode_add(root, "conn", svc_conn_handler) == NULL)
		|| (russ_svcnode_add(root, "daytime", svc_daytime_handler) == NULL)
		|| (russ_svcnode_add(root, "discard", svc_discard_handler) == NULL)
		|| (russ_svcnode_add(root, "echo", svc_echo_handler) == NULL)
		|| (russ_svcnode_add(root, "env", svc_env_handler) == NULL)
		|| (russ_svcnode_add(root, "request", svc_request_handler) == NULL)
		|| (russ_svcnode_add(root, "whoami", svc_whoami_handler) == NULL)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK, RUSS_SVR_LIS_SD_DEFAULT)) == NULL)
		|| (russ_svr_set_auto_switch_user(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);
}
