/*
** bin/rusrv_pnet.c
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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#include "russ_conf.h"
#include "russ.h"

#define DEFAULT_RELAY_ADDR	"+/ssh"
#define MAX_HOSTS		(1024)

struct hostslist {
	char	*hosts[MAX_HOSTS];
	int	nhosts;
	int	next;
};

/* global */
struct russ_conf	*conf = NULL;
char			*hostsfilename = NULL;
struct hostslist	hostslist;

char	*HELP = 
"Provides access to local/remote targets (e.g., user@host) using a\n"
"relay (e.g., ssh service).\n"
"\n"
"count\n"
"    Output the number of targets registered.\n"
"\n"
"first/... <args>\n"
"\n"
"host/<user@host>/... <args>\n"
"    Connect to service ... at target (i.e., user@host) verified\n"
"    by a lookup into the hostsfile list. Only available if a\n"
"    hostsfile was given at startup.\n"
"\n"
"id/<index>/... <args>\n"
"    Connect to service ... at target identified by a lookup into\n"
"    the hostsfile list at <index>. A negative index starts at the\n"
"    last entry (1 is the last entry). An index starting with :\n"
"    loops around to continue the lookup.\n"
"\n"
"net/... <args>\n"
"\n"
"next/... <args>\n"
"    Connect to the 'next' target selected from the hostsfile\n"
"    list. Each call bumps to 'next' and wraps to 0 as needed.\n"
"\n"
"random/... <args>\n"
"    Connect to a randomly selected target from the hostsfile\n"
"    list.\n";

int
switch_user(struct russ_conn *conn) {
	uid_t	uid;
	gid_t	gid;

	uid = conn->cred.uid;
	gid = conn->cred.gid;

#if 0
	if (uid == 0) {
		russ_conn_fatal(conn, "error: cannot run for root (uid of 0)", -1);
		exit(0);
	}
#endif

	/* switch user */
	if (russ_switch_user(uid, gid, 0, NULL) < 0) {
		russ_conn_fatal(conn, "error: cannot switch user", -1);
		exit(0);
	}
	return 0;
}

/**
* Patch conn->spath based on a target which answer.
*
* Convert:
*	first/... -> <relay_addr>/<userhost>/...
* where <userhost> is select because it answers.
*
* @param conn		connection object
* @return		0 on success; -1 on failure
*/
int
_first_patch(struct russ_conn *conn) {
	return -1;
}

/**
* Patch conn->spath based on original "host" spath.
*
* Convert:
*	host/<userhost>/... -> <relay_addr>/<userhost>/...
*
* @param conn		connection object
* @return		0 on success; -1 on failure
*/
int
_host_patch(struct russ_conn *conn) {
	char	*p, *spath_tail, *userhost, *relay_addr;
	char	new_spath[RUSS_MAX_PATH_LEN];
	int	i;

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[6];
	if ((p = index(userhost, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	spath_tail = strdup(p+1);
	p[0] = '\0'; /* terminate userhost */

	for (i = 0; i < hostslist.nhosts; i++) {
		if (strcmp(userhost, hostslist.hosts[i]) == 0) {
			break;
		}
	}
	if (i == hostslist.nhosts) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, spath_tail) < 0) {
		russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	return 0;
}

/**
* Patch conn->spath based on original "id" spath.
*
* Convert:
*	id/<index>/... -> <relay_addr>/<userhost>/...
*
* @param conn		connection object
* @return		0 on success; -1 on failure
*/
int
_id_patch(struct russ_conn *conn) {
	char	*p, *spath_tail, *s, *userhost, *relay_addr;
	char	new_spath[RUSS_MAX_PATH_LEN];
	int	i, idx, wrap = 0;

	/* extract and validate user@host and new_spath */
	s = &conn->req.spath[4];
	if ((p = index(s, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	spath_tail = strdup(p+1);
	p[0] = '\0'; /* terminate userhost */
	if (s[0] == ':') {
		s = &s[1];
		wrap = 1;
	}
	if (sscanf(s, "%d", &idx) != 1) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* wrap if requested */
	if (wrap) {
		idx = idx % hostslist.nhosts;
	}

	/* negative indexes */
	if ((idx < 0) && (-idx <= hostslist.nhosts)) {
		idx = hostslist.nhosts+idx;
	}

	/* validate */
	if ((idx < 0) || (idx >= hostslist.nhosts)) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	userhost = hostslist.hosts[idx];

	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, spath_tail) < 0) {
		russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	return 0;
}

/**
* Patch conn->spath to use provided userhost.
*
* Convert:
*	net/<userhost>/... -> <relay_addr>/<userhost>/...
*
* @param conn		connection object
* @return		0 on success; -1 on failure
*/
int
_net_patch(struct russ_conn *conn) {
	char	*p, *spath_tail, *userhost, *relay_addr;
	char	new_spath[RUSS_MAX_PATH_LEN];

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[5];
	if ((p = index(userhost, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	spath_tail = strdup(p+1);
	p[0] = '\0'; /* terminate userhost */

	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, spath_tail) < 0) {
		russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	return -1;
}

/**
* Patch conn->spath to use the 'next' id value.
*
* Convert:
*	next/... -> <relay_addr>/<userhost>/...
* where userhost is selected using a 'next' counter.
*
* @param conn		connection object
* @return		0 on success; -1 on failure
*/
int
_next_patch(struct russ_conn *conn) {
	char	new_spath[RUSS_MAX_PATH_LEN];
	int	idx;

	idx = hostslist.next;
	if (snprintf(new_spath, sizeof(new_spath), "/id/%d/%s", idx, &conn->req.spath[6]) < 0) {
		russ_conn_fatal(conn, "error: spath is too large", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	return _id_patch(conn);
}

/**
* Patch conn->spath to use a random id value.
*
* Convert:
*	random/... -> <relay_addr>/<userhost>/...
* where userhost is selected at random from the list of targets.
*
* @param conn		connection object
* @return		0 on success; -1 on failure
*/
int
_random_patch(struct russ_conn *conn) {
	char	new_spath[RUSS_MAX_PATH_LEN];
	int	idx;

	idx = (random()/(double)RAND_MAX)*hostslist.nhosts;
	if (snprintf(new_spath, sizeof(new_spath), "/id/%d/%s", idx, &conn->req.spath[8]) < 0) {
		russ_conn_fatal(conn, "error: spath is too large", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	return _id_patch(conn);
}

/*
* Alternate accept handler for certain spaths.

* The spaths: /id/, /host/, /first/, /next/, /net/, and /random/ are
* treated specially before an accept is done. If none of the special
* spaths are found, the default accept handler is called (for
* further processing).
*
* @param self		connection object
* @return		0 on success; -1 on failure
*/

int
alt_accept_handler(struct russ_conn *self) {
	struct russ_conn	*conn;
	struct russ_request	*req;

	/* ordered by expected use */
	req = &(self->req);
	if (strncmp(req->spath, "/id/", 4) == 0) {
		_id_patch(self);
	} else if (strncmp(req->spath, "/host/", 6) == 0) {
		_host_patch(self);
	} else if (strncmp(req->spath, "/first/", 7) == 0) {
		_first_patch(self);
	} else if (strncmp(req->spath, "/net/", 5) == 0) {
		_net_patch(self);
	} else if (strncmp(req->spath, "/next/", 6) == 0) {
		_next_patch(self);
	} else if (strncmp(req->spath, "/random/", 8) == 0) {
		_random_patch(self);
	} else {
		return russ_standard_accept_handler(self);
	}

	/* dial next service and splice */
	if ((switch_user(self) < 0)
		|| ((conn = russ_dialv(-1, req->op, req->spath, req->attrv, req->argv)) == NULL)) {
		return -1;
	}
	return russ_conn_splice(self, conn);
}

/**
* Service request only if it is for "/".
*/
void
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	int			outfd;
	int			i;

	outfd = conn->fds[1];
	req = &(conn->req);
	if (strcmp(req->spath, "/count") == 0) {
		russ_dprintf(outfd, "%d", hostslist.nhosts);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else if ((strcmp(req->spath, "/first/") == 0)
		|| (strcmp(req->spath, "/host/") == 0)
		|| (strcmp(req->spath, "/id/") == 0)
		|| (strcmp(req->spath, "/net/") == 0)
		|| (strcmp(req->spath, "/next/") == 0)
		|| (strcmp(req->spath, "/random/") == 0)) {
		/* nothing */
	} else if (strcmp(req->op, "execute") == 0) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	} else if (strcmp(req->op, "help") == 0) {
        	russ_dprintf(outfd, "%s", HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else if (strcmp(req->op, "list") == 0) {
		if (strcmp(req->spath, "/") == 0) {
			russ_dprintf(outfd, "first\nhost\nid\nnet\nnext\nrandom\n");
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else if (strcmp(req->spath, "/host") == 0) {
			if (hostslist.nhosts == 0) {
				russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			} else {
				for (i = 0; i < hostslist.nhosts; i++) {
					russ_dprintf(outfd, "%s\n", hostslist.hosts[i]);
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			}
		} else if (strcmp(req->spath, "/id") == 0) {
			if (hostslist.nhosts == 0) {
				russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			} else {
				for (i = 0; i < hostslist.nhosts; i++) {
					russ_dprintf(outfd, "%d\n", i);
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			}
		} else if ((strcmp(req->spath, "/net") == 0)) {
			//russ_conn_fatal(conn, RUSS_MSG_UNSPEC_SERVICE, RUSS_EXIT_SUCCESS);
			russ_conn_fatal(conn, "error: unspecified service", RUSS_EXIT_SUCCESS);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
	}
	exit(0);
}

struct russ_conn *
alt_answer_handler(struct russ_listener *self, russ_deadline deadline) {
	struct russ_conn	*conn;

	if ((conn = russ_listener_answer(self, deadline)) != NULL) {
		hostslist.next = (hostslist.next+1 >= hostslist.nhosts) ? 0 : hostslist.next+1;
		random(); /* tickle */
	}
	return conn;
}

int
load_hostsfile(char *filename) {
	char	*line;
	int	i;
	size_t	line_size;
	ssize_t	nbytes;
	FILE	*f;

	if ((f = fopen(filename, "r")) == NULL) {
		return -1;
	}
	for (i = 0, hostslist.nhosts = 0; i < MAX_HOSTS; i++) {
		line = NULL;
		if ((nbytes = getline(&line, &line_size, f)) < 0) {
			break;
		}
		if (line[nbytes-1] == '\n') {
			line[nbytes-1] = '\0';
		}
		if ((line[0] == '\0') || (line[0] == '#')) {
			/* ignore empty and comment lines */
			free(line);
			continue;
		}
		hostslist.hosts[i] = line;
		hostslist.nhosts++;
	}
	hostslist.next = -1;
	return 0;
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_pnet [<conf options>] -- <hostsfile>\n"
"\n"
"Routes connections over the network to a fixed set of targets\n"
"identified by index or hostname.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	srandom(time(NULL));
	hostslist.nhosts = 0;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (argc < 2) {
		fprintf(stderr, "error: missing hosts file\n");
		exit(1);
	}
	hostsfilename = argv[1];
	if (load_hostsfile(hostsfilename) < 0) {
		fprintf(stderr, "error: could not load hosts file\n");
		exit(1);
	}

	lis = russ_announce(russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid()));
	if (lis == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_listener_loop(lis, alt_answer_handler, alt_accept_handler, master_handler);
	exit(0);
}
