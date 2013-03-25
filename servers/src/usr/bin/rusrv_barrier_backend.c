/*
** bin/rusrv_barrier.c
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
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "russ.h"

#define MAX( a,b ) (((a) > (b)) ? (a) : (b))

struct barrier_item {
	struct russ_conn	*conn;
	char			*tag;
};

struct barrier {
	char			*saddr;
	int			count;
	time_t			timeout;
	time_t			due_time;
	struct barrier_item	*items;
	int			nitems;
};

struct barrier	*barrier;

/**
* Create barrier object.
*
* @param saddr		service address
* @param count		# of clients to sync
* @param timeout	time-to-live of barrier
* @return		barrier object or NULL
*/
struct barrier *
new_barrier(char *saddr, int count, time_t timeout) {
	struct barrier	*barr;

	if ((barr = malloc(sizeof(struct barrier))) == NULL) {
		return NULL;
	}
	if ((barr->saddr = strdup(saddr)) == NULL) {
		goto free_barr;
	}
	if ((barr->items = malloc(sizeof(struct barrier_item)*count)) == NULL) {
		goto free_saddr;
	}
	barr->nitems = 0;
	barr->count = count;
	barr->timeout = timeout;
	barr->due_time = time(NULL)+timeout;

	return barr;

free_saddr:
	free(barr->saddr);
free_barr:
	free(barr);
	return NULL;
}

/**
* Release barrier. Send byte value and close fds.
*
* @param conn	connection object
*/
void
release_barrier(char ch) {
	struct russ_conn	*conn;
	int			fd, i;

	for (i = 0; i < barrier->nitems; i++) {
		conn = barrier->items[i].conn;
		if ((conn) && (fd = conn->fds[1]) > -1) {
			write(fd, &ch, 1);
			russ_conn_close(conn);
			barrier->items[i].conn = russ_conn_free(conn);
			free(barrier->items[i].tag);
		}
	}
	barrier->nitems = 0;
	free(barrier->items);
}

void
print_service_usage(struct russ_conn *conn) {
	russ_dprintf(conn->fds[2],
"Barrier service.\n"
"\n"
"count         print # of waiters expected\n"
"kill          kill barrier and release all waiters\n"
"tags          print tags of waiters\n"
"ttl           print time-to-live remaining\n"
"wait [<tag>]  wait on barrier; register optional tag\n"
"wcount        print # of waiters currently waiting\n"
);
}

/**
* Request handler.
*
* @param conn	connection object
* @return	0 on success, -1 on error
*/
int
req_handler(struct russ_conn *conn) {
	char	buf[2048];
	time_t	due_time;
	int	outfd;
	int	i;

	outfd = conn->fds[1];
	switch (conn->req.opnum) {
	case RUSS_OPNUM_EXECUTE:
		if (strcmp(conn->req.spath, "/wait") == 0) {
			if ((conn->req.argv) && (conn->req.argv[0] != NULL)) {
				barrier->items[barrier->nitems].tag = strdup(conn->req.argv[0]);
			} else {
				/* use pid as tag, if available */
				snprintf(buf, sizeof(buf), "%ld", conn->creds.pid);
				barrier->items[barrier->nitems].tag = strdup(buf);
			}
			barrier->items[barrier->nitems++].conn = conn;
			//close(conn->fds[0]);
			//conn->fds[0] = -1;
			close(conn->fds[2]);
			conn->fds[2] = -1;

			if (barrier->nitems == barrier->count) {
				release_barrier('r');
				//cleanup_exit(0);
				exit(0);
			}
		} else {
			if (strcmp(conn->req.spath, "/count") == 0) {
				russ_dprintf(outfd, "%d\n", barrier->count);
			} else if (strcmp(conn->req.spath, "/wcount") == 0) {
				russ_dprintf(outfd, "%d\n", barrier->nitems);
			} else if (strcmp(conn->req.spath, "/kill") == 0) {
				release_barrier('k');
				exit(0);
			} else if (strcmp(conn->req.spath, "/tags") == 0) {
				for (i = 0; i < barrier->nitems; i++) {
					russ_dprintf(outfd, "%s\n", barrier->items[i].tag);
				}
			} else if (strcmp(conn->req.spath, "/ttl") == 0) {
				if (barrier->timeout == -1) {
					russ_dprintf(outfd, "infinite\n");
				} else {
					russ_dprintf(outfd, "%d\n", barrier->due_time-time(NULL));
				}
			} else {
				russ_dprintf(conn->fds[2], "error: unknown service\n");
			}
			goto close_conn;
		}
		break;
	case RUSS_OPNUM_HELP:
		print_service_usage(conn);
		goto close_conn;
		break;
	case RUSS_OPNUM_LIST:
		russ_dprintf(conn->fds[1], "count\nkill\ntags\nttl\nwait\nwcount\n");
		goto close_conn;
		break;
	default:
		russ_dprintf(conn->fds[2], "error: unknown operation\n");
		goto close_conn;
	}
	return 0;

close_conn:
	russ_conn_close(conn);
	return -1;
}

void
print_usage(char **argv) {
	char	*prog_name;

	prog_name = basename(argv[0]);
	fprintf(stderr,
"usage: %s [<options>] <saddr> <count>\n"
"\n"
"Russ-based sync worker. Should be started by sync master.\n"
"\n"
"Where:\n"
"<saddr>		service address (file path)\n"
"<count>		# of waiters to wait for\n"
"\n"
"Options:\n"
"-m <mode>	file mode of service file (octal); default is 0666\n"
"-g <gid>	group id of service file; default is creator\n"
"-u <uid>	user id of service file; default is creator\n"
"-t <timeout>	maximum # of seconds to wait\n"
"--me		count this connection as a waiter\n",
		prog_name);
}

int
main(int argc, char **argv) {
	struct russ_lis		*lis;
	struct russ_conn	*conn;
	struct russ_req		*req;
	struct pollfd		poll_fds[1];
	char			*saddr;
	mode_t			mode;
	uid_t			uid;
	gid_t			gid;
	int			count;
	time_t			timeout;
	int			poll_timeout;
	char			*arg;
	int			argi;
	int			me;
	int			rv;

	signal(SIGCHLD, SIG_IGN);

	/* parse command line */
	me = 0;
	mode = 0666;
	uid = getuid();
	gid = getgid();
	timeout = -1;
	argi = 1;
	while (argi < argc) {
		arg = argv[argi++];
		if ((strcmp(arg, "-h") == 0) || (strcmp(arg, "--help") == 0)) {
			print_usage(argv);
			exit(0);
		} else if ((strcmp(arg, "-g") == 0) && (argi < argc)) {
			arg = argv[argi++];
			if (sscanf(arg, "%d", (int *)&gid) < 0) {
				goto error_bad_arg;
			}
		} else if ((strcmp(arg, "-m") == 0) && (argi < argc)) {
			arg = argv[argi++];
			if (sscanf(arg, "%od", (int *)&gid) < 0) {
				goto error_bad_arg;
			}
		} else if ((strcmp(arg, "-t") == 0) && (argi < argc)) {
			arg = argv[argi++];
			if (sscanf(arg, "%d", (int *)&timeout) < 0) {
				goto error_bad_arg;
			}
		} else if ((strcmp(arg, "-u") == 0) && (argi < argc)) {
			arg = argv[argi++];
			if (sscanf(arg, "%d", (int *)&uid) < 0) {
				goto error_bad_arg;
			}
		} else if (strcmp(arg, "--me") == 0) {
			me = 1;
		} else {
			argi--;
			break;
		}
	}
	if ((argc-argi != 2)
		|| ((saddr = strdup(argv[argi])) == NULL)
		|| (sscanf(argv[argi+1], "%d", (int *)&count) < 0)) {
		goto error_bad_arg;
	}

	/* set up barrier; announce service */
	barrier = new_barrier(saddr, count, timeout);
	if ((lis = russ_announce(saddr, mode, uid, gid)) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}

	/* listen for and process requests */
	poll_fds[0].fd = lis->sd;
	poll_fds[0].events = POLLIN;
	while (barrier->nitems < barrier->count) {
		if (barrier->timeout == -1) {
			poll_timeout = -1;
		} else {
			poll_timeout = (barrier->due_time-time(NULL));
			poll_timeout = MAX(0, poll_timeout)*1000;
		}
		rv = poll(poll_fds, 1, poll_timeout);
		if (rv == 0) {
			break;
		} else if (rv < 0) {
			if (errno != EINTR) {
				exit(-1);
			};
		} else {
			if (poll_fds[0].revents && POLLIN) {
				if ((conn = russ_lis_accept(lis, RUSS_TIMEOUT_NEVER)) == NULL) {
					continue;
				}
				if ((russ_conn_await_request(conn) < 0)
					|| (russ_conn_answer(conn, NULL, NULL) < 0)) {
					russ_conn_close(conn);
					conn = russ_conn_free(conn);
					continue;
				}
				req_handler(conn);
			}
		}
	}
	release_barrier('t');
	exit(0);

error_bad_arg:
	fprintf(stderr, "error: bad argument(s)\n");
	exit(-1);
	
}
