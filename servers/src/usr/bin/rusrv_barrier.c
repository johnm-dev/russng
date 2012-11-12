/*
* bin/rusrv_barrier.c
*
* This file contains backend and frontend sections. The frontend
* handles requests to create a barrier, but the actual barrier
* service is handled by the backend.
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

#include "russ_conf.h"
#include "russ.h"

#define MAX( a,b ) (((a) > (b)) ? (a) : (b))

/* structs */
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

/* globals */
struct russ_conf	*conf = NULL;
struct barrier		*barrier = NULL;

/*
* backend
*/

char *BACKEND_HELP =
"Barrier service instance.\n"
"\n"
"cancel\n"
"    Cancel barrier and release all waiters.\n"
"\n"
"count\n"
"    Print number of waiters expected.\n"
"\n"
"tags\n"
"    Print tags of waiters.\n"
"\n"
"ttl\n"
"    Print time-to-live remaining.\n"
"\n"
"wait [<tag>]\n"
"    Wait on barrier; register optional tag.\n"
"\n"
"wcount\n"
"    Print # of waiters currently waiting.\n";

/**
* Create barrier object.
*
* @param saddr		service address
* @param count		# of clients to sync
* @param timeout	time-to-live of barrier
* @return		barrier object or NULL
*/
struct barrier *
backend_new_barrier(char *saddr, int count, time_t timeout) {
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
backend_release_barrier(char ch) {
	struct russ_conn	*conn;
	int			fd, i;

	for (i = 0; i < barrier->nitems; i++) {
		conn = barrier->items[i].conn;
		if ((conn) && (fd = conn->fds[1]) > -1) {
			write(fd, &ch, 1);
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			russ_conn_close(conn);
			barrier->items[i].conn = russ_conn_free(conn);
			free(barrier->items[i].tag);
		}
	}
	barrier->nitems = 0;
	free(barrier->items);
}

void
backend_add_waiter(struct russ_conn *conn) {
	char	buf[2048];

	if ((conn->req.argv) && (conn->req.argv[0] != NULL)) {
		barrier->items[barrier->nitems].tag = strdup(conn->req.argv[0]);
	} else {
		/* use pid as tag, if available */
		snprintf(buf, sizeof(buf), "%ld", conn->creds.pid);
		barrier->items[barrier->nitems].tag = strdup(buf);
	}
	barrier->items[barrier->nitems++].conn = conn;
}

/**
* Main backend request handler.
*
* Handle each service of which the /wait service is unique because
* it does not release the connection(s), except when the desired
* number of waiters is reached. An 'r' is sent to outfd on normal
* release, 'c' on cancel, and 't' (in backend_loop()) on timeout.
* In all cases, the connection exit value is 0.
*
* @param conn	connection object
*/
void
backend_master_handler(struct russ_conn *conn) {
	time_t	due_time;
	int	errfd, outfd;
	int	i;

	errfd = conn->fds[2];
	outfd = conn->fds[1];
	if (strcmp(conn->req.op, "execute") == 0) {
		if (strcmp(conn->req.spath, "/wait") == 0) {
			backend_add_waiter(conn);
			//russ_close_fds(&conn->fds[0], 1);
			//russ_close_fds(&conn->fds[2], 1);
			if (barrier->nitems == barrier->count) {
				backend_release_barrier('r');
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
				goto cleanup_and_exit;
			}
		} else {
			if (strcmp(conn->req.spath, "/cancel") == 0) {
				backend_release_barrier('c');
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
				goto cleanup_and_exit;
			} else if (strcmp(conn->req.spath, "/count") == 0) {
				russ_dprintf(outfd, "%d\n", barrier->count);
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			} else if (strcmp(conn->req.spath, "/tags") == 0) {
				for (i = 0; i < barrier->nitems; i++) {
					russ_dprintf(outfd, "%s\n", barrier->items[i].tag);
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			} else if (strcmp(conn->req.spath, "/ttl") == 0) {
				if (barrier->timeout == -1) {
					russ_dprintf(outfd, "infinite\n");
				} else {
					russ_dprintf(outfd, "%d\n", barrier->due_time-time(NULL));
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			} else if (strcmp(conn->req.spath, "/wcount") == 0) {
				russ_dprintf(outfd, "%d\n", barrier->nitems);
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			} else {
				russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			}
			goto close_conn;
		}
	} else if (strcmp(conn->req.op, "help") == 0) {
		russ_dprintf(outfd, "%s", BACKEND_HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		goto close_conn;
	} else if (strcmp(conn->req.op, "list") == 0) {
		if (strcmp(conn->req.spath, "/") == 0) {
			russ_dprintf(outfd, "cancel\ncount\ntags\nttl\nwait\nwcount\n");
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
		goto close_conn;
	} else {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
		goto close_conn;
	}

close_conn:
	russ_conn_close(conn);
	return;

cleanup_and_exit:
	//remove(conn->saddr);
	exit(0);
}

int
backend_loop(struct russ_conn *fconn, char *saddr, mode_t mode, uid_t uid, gid_t gid, int count, time_t timeout) {
	struct russ_lis		*lis;
	struct russ_conn	*bconn;
	struct russ_req		*req;
	struct pollfd		poll_fds[2];
	int			poll_timeout;
	int			rv;
	int			ferrfd, foutfd;

	ferrfd = fconn->fds[2];
	foutfd = fconn->fds[1];

	/* set up backend barrier; announce service */
	barrier = backend_new_barrier(saddr, count, timeout);
	if ((lis = russ_announce(saddr, mode, uid, gid)) == NULL) {
		russ_conn_fatal(fconn, "error: cannot announce service", RUSS_EXIT_FAILURE);
		return -1;
	}

	/* add initial connection */
	backend_add_waiter(fconn);

	/* listen for and process requests */
	poll_fds[0].fd = lis->sd;
	poll_fds[0].events = POLLIN;
	poll_fds[1].fd = fconn->fds[1];
	poll_fds[1].events = POLLHUP;

	while (barrier->nitems < barrier->count) {
		if (barrier->timeout == -1) {
			poll_timeout = -1;
		} else {
			poll_timeout = (barrier->due_time-time(NULL));
			poll_timeout = MAX(0, poll_timeout)*1000;
		}
		rv = poll(poll_fds, 2, poll_timeout);
		if (rv == 0) {
			backend_release_barrier('t');
			goto cleanup_and_exit;
		} else if (rv < 0) {
			if (errno != EINTR) {
				backend_release_barrier('e');
				goto cleanup_and_exit;
			};
		} else {
			if (poll_fds[0].revents & POLLIN) {
				if ((bconn = russ_lis_answer(lis, RUSS_DEADLINE_NEVER)) == NULL) {
					continue;
				}
				if ((russ_conn_await_request(bconn, RUSS_DEADLINE_NEVER) < 0)
					|| (russ_conn_accept(bconn, 0, NULL, NULL) < 0)) {
					russ_conn_close(bconn);
					bconn = russ_conn_free(bconn);
					continue;
				}
				backend_master_handler(bconn); /* exits if count reached */
			}
			if (poll_fds[1].revents & (POLLHUP|POLLNVAL|POLLERR)) {
				backend_release_barrier('c');
				goto cleanup_and_exit;
			}
		}
	}
	backend_release_barrier('r');

cleanup_and_exit:
	remove(saddr);
	exit(0);
}

/*
* frontend
*/

char *FRONTEND_HELP =
"Barrier service creator.\n"
"\n"
"Once the barrier is actually created, different services are\n"
"effective.\n"
"\n"
"generate [<name>]\n"
"    Generate a unique saddr to provide to the barrier service\n"
"    (see /new). An optional name can be used to generate a\n"
"    predetermined rendez-vous point (warning: this should be\n"
"    well chosen to avoid naming conflicts).\n"
"\n"
"new <saddr> <count> [-m <mode>] [-u <uid>] [-g <gid>] [-t <timeout>]\n"
"    Create a barrier at <saddr> with a liftime of <timeout>.\n"
"    By default, the barrier is accessible only to the creator.\n"
"    This can be overridden for the file mode, and with\n"
"    sufficient privileges, the ownership can also be set.\n";

/**
* Start new barrier backend instance.
*
* @param conn		connection object
*/
void
svc_new_handler(struct russ_conn *conn) {
	char	**argv, *arg;
	long	mode, uid, gid;
	long	timeout;
	char	*saddr;
	int	count;
	int	errfd, outfd;
	int	argi;

	errfd = conn->fds[2];
	outfd = conn->fds[1];

	mode = 0600;
	uid = conn->creds.uid;
	gid = conn->creds.gid;
	count = 0;
	timeout = 600;

	/* parse args */
	argv = conn->req.argv;
	if ((argv == NULL) || (argv[0] == NULL) || (argv[1] == NULL)) {
		goto error_bad_args;
	} else {
		saddr = strdup(argv[0]); /* dup in case conn gets closed/freed */
		if (sscanf(argv[1], "%d", &count) < 0) {
			goto error_bad_args;
		}
	}

	for (argi = 2; argv[argi] != NULL; argi++) {
		arg = argv[argi++];

		if ((strcmp(arg, "-g") == 0)
			&& (getuid() == 0)
			&& (argv[argi] != NULL)
			&& (sscanf(argv[argi], "%ld", &gid) >= 0)) {
			argi++;
		} else if ((strcmp(arg, "-m") == 0)
			&& (argv[argi] != NULL)
			&& (sscanf(argv[argi], "%ld", &mode) >= 0)) {
			argi++;
		} else if ((strcmp(arg, "-t") == 0)
			&& (argv[argi] != NULL)
			&& (sscanf(argv[argi], "%ld", &timeout) >= 0)) {
			argi++;
		} else if ((strcmp(arg, "-u") == 0)
			&& (getuid() == 0)
			&& (argv[argi] != NULL)
			&& (sscanf(argv[argi], "%ld", &uid) >= 0)) {
			argi++;
		} else {
			goto error_bad_args;
		}
	}

	backend_loop(conn, saddr, mode, uid, gid, count, timeout);
	free(saddr);
	return;

error_bad_args:
	russ_conn_fatal(conn, RUSS_MSG_BAD_ARGS, RUSS_EXIT_FAILURE);
}

unsigned long
get_random(void) {
	int		f;
	unsigned long	v;

	f = open("/dev/urandom", O_RDONLY);
	read(f, &v, sizeof(v));
	close(f);
	return v;
}
/**
* Main request handler.
*/
void
master_handler(struct russ_conn *conn) {
	struct russ_req	*req;
	int		errfd, outfd;

	req = &conn->req;
	errfd = conn->fds[2];
	outfd = conn->fds[1];

	/* switch to user creds */

	if ((chdir("/tmp") < 0)
		|| (russ_switch_user(conn->creds.uid, conn->creds.gid, 0, NULL) < 0)) {
		russ_conn_fatal(conn, "error: cannot set up", RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (strcmp(req->op, "execute") == 0) {
		if (strcmp(req->spath, "/generate") == 0) {
			char	hostname[1024];

			gethostname(hostname, sizeof(hostname));
			russ_dprintf(outfd, "%s-%012d-%04ld", hostname, 0, get_random() % 10000);
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else if (strcmp(req->spath, "/new") == 0) {
			svc_new_handler(conn);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else if (strcmp(req->op, "help") == 0) {
		russ_dprintf(outfd, "%s", FRONTEND_HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else if (strcmp(req->op, "list") == 0) {
		if (strcmp(req->spath, "/") == 0) {
			russ_dprintf(outfd, "generate\nnew\n");
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
	char	*prog_name;

	prog_name = basename(argv[0]);
	fprintf(stderr,
"usage: rusrv_%s [<conf options>]\n"
"\n"
"Front-end barrier service. Instantiates backend barrier service.\n",
		prog_name);
}

int
main(int argc, char **argv) {
	struct russ_lis	*lis;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN); /* no errors on failed writes */

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(-1);
	}

	lis = russ_announce(russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid()));
	if (lis == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_lis_loop(lis, NULL, NULL, master_handler);
	exit(0);
}
