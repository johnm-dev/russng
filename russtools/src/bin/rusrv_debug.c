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

#include "russ.h"

int
_error_handler(struct russ_conn *conn, char *msg) {
	russ_dprintf(conn->fds[2], msg);
}

int
_chargen_handler(struct russ_conn *conn) {
	char	buf[] = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ ";
	char	off;

	off = 0;
	while (russ_dprintf(conn->fds[1], "%.72s\n", &(buf[off])) >= 0) {
		off++;
		if (off > 94) {
			off = 0;
		}
		usleep(100000);
	}
}

int
_daytime_handler(struct russ_conn *conn) {
	char		buf[1024];
	time_t		now;
	struct tm	*now_tm;

	now = time(NULL);
	now_tm = localtime(&now);
	strftime(buf, sizeof(buf), "%A, %B %d, %Y %T-%Z", now_tm);
	russ_dprintf(conn->fds[2], "%s\n", buf);
}

static void
print_discard_stats(int fd, time_t elapsed, long total) {
	float	total_mb;

	total_mb = (float)total/(1<<20);
	russ_dprintf(fd, "total (MB): %.4f  elapsed (s): %d  throughput (MB/s): %.4f\n",
		total_mb, elapsed, total_mb/elapsed);
}

int
_discard_handler(struct russ_conn *conn) {
	char	*buf;
	int	buf_size;
	time_t	t0, last_t1;
	long	n, total;

	/* 8MB */
	buf_size = 1<<23;
	if ((buf = malloc(buf_size)) == NULL) {
		return _error_handler(conn, "error: cannot allocate buffer\n");
	}
	if ((conn->req.argc) && (strcmp(conn->req.argv[0], "--perf") == 0)) {
		t0 = time(NULL);
		last_t1 = t0;
		total = 0;
		while ((n = russ_read(conn->fds[0], buf, buf_size)) > 0) {
			total += n;
			if (time(NULL)-last_t1 > 2) {
				last_t1 = time(NULL);
				print_discard_stats(conn->fds[2], last_t1-t0, total);
			}
		}
		print_discard_stats(conn->fds[2], time(NULL)-t0, total);
	} else {
		while ((n = russ_read(conn->fds[0], buf, buf_size)) > 0);
	}
	return 0;
}

int
_echo_handler(struct russ_conn *conn) {
	char	buf[1024];
	ssize_t	n;

	while ((n = russ_read(conn->fds[0], buf, sizeof(buf))) > 0) {
		russ_writen(conn->fds[1], buf, n);
	}
	return 0;
}

int
_request_handler(struct russ_conn *conn) {
	russ_dprintf(conn->fds[2], "error: not implemented yet\n");
	return 0;
}

int
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	int			rv;

	req = &(conn->req);
	if (strcmp(req->op, "execute") == 0) {
		if (strcmp(req->spath, "/chargen") == 0) {
			rv = _chargen_handler(conn);
		} else if (strcmp(req->spath, "/daytime") == 0) {
			rv = _daytime_handler(conn);
		} else if (strcmp(req->spath, "/discard") == 0) {
fprintf(stderr, "discarding...\n");
			rv = _discard_handler(conn);
		} else if (strcmp(req->spath, "/echo") == 0) {
			rv = _echo_handler(conn);
		} else if (strcmp(req->spath, "/request") == 0) {
			rv = _request_handler(conn);
		} else {
			rv = _error_handler(conn, "error: unknown service\n");
		}
	} else {
		rv = _error_handler(conn, "error: unsupported operation\n");
	}
	return rv;
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_debug <saddr> [...]\n"
"\n"
"Provides services useful for debugging. Unless otherwise stated,\n"
"stdin, stdout, and stderr all refer to the file descriptor triple\n"
"that is returned from a russ_dial call.\n"
"\n"
".../chargen	character generator outputting to stdout; follows\n"
"		the RFC 864 protocol sequence\n"
".../daytime	returns the date and time to the stdout\n"
".../discard [--perf]\n"
"		discards all data received from stdin; if --perf is\n"
"		specified, performance feedback is provide to\n"
"		stderr, otherwise there is none\n"
".../echo	simple echo service; receives from stdin and outputs\n"
"		to stdout\n"
".../request	display the request information at the server\n"
"		stdout\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;
	char			*saddr;

	signal(SIGCHLD, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(-1);
	}
	saddr = argv[1];

	if ((lis = russ_announce(saddr, 0666, getuid(), getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_loop(lis, master_handler);
}
