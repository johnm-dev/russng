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

#define MAX_HOSTS	(1024)

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
"Provides access to remote user@host using ssh.\n"
"\n"
"hid/<hid>/... <args>\n"
"    Connect to service ... at user@host identified by a lookup\n"
"    into the hostsfile list where <hid> is the index. Only\n"
"    available if a hostsfile was given at startup.\n"
"\n"
"host/<user@host>/... <args>\n"
"    Connect to service ... at user@host verified by a lookup into\n"
"    the hostsfile list. Only available if a hostsfile was given\n"
"    at startup.\n"
"\n"
"next/... <args>\n"
"    Connect to the 'next' host selected from the hostsfile list.\n"
"    Each call bumps to 'next' and wraps to 0 as needed.\n"
"\n"
"random/... <args>\n"
"    Connect to a randomly select host selected from the hostsfile\n"
"    list.\n";

int
switch_user(struct russ_conn *conn) {
	uid_t	uid;
	gid_t	gid;

	uid = conn->cred.uid;
	gid = conn->cred.gid;

	if (uid == 0) {
		russ_conn_fatal(conn, "error: cannot run for root (uid of 0)", -1);
		exit(0);
	}

	/* switch user */
	if ((setgid(gid) < 0) || (setuid(uid) < 0)) {
		russ_conn_fatal(conn, "error: cannot switch user", -1);
		exit(0);
	}
	return 0;
}

/**
* Patch conn->spath based on original "hid" spath.
*
* .../hid/<index>/... -> .../<relay_method>/<userhost>/...
*/
int
_hid_patch(struct russ_conn *conn) {
	char	*p, *spath_tail, *hids, *userhost;
	char	tmp[16384];
	int	i, hid;

	/* extract and validate user@host and new_spath */
	hids = &conn->req.spath[5];
	if ((p = index(hids, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	spath_tail = strdup(p);
	p[0] = '\0'; /* terminate userhost */
	if (sscanf(hids, "%d", &hid) < 0) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	if ((hid < 0) || (hid >= hostslist.nhosts)) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	userhost = hostslist.hosts[hid];

	if (snprintf(tmp, sizeof(tmp), "/%s/%s/%s", "+ssh", userhost, spath_tail) < 0) {
		russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(p);
	conn->req.spath = strdup(tmp);
	return 0;
}

/**
* Patch conn->spath based on original "host" spath.
*
* .../host/<index>/... -> .../<relay_method>/<userhost>/...
*/
int
_host_patch(struct russ_conn *conn) {
	char	*p, *spath_tail, *userhost;
	char	tmp[16384];
	int	i;

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[6];
	if ((p = index(userhost, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	spath_tail = strdup(p);
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

	if (snprintf(tmp, sizeof(tmp), "/%s/%s/%s", "+ssh", userhost, spath_tail) < 0) {
		russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(p);
	conn->req.spath = strdup(tmp);
	return 0;
}

int
_next_patch(struct russ_conn *conn) {
	char	new_spath[16384];
	int	hid;

	hid = hostslist.next;
	if (snprintf(new_spath, sizeof(new_spath), "/hid/%d/%s", hid, &conn->req.spath[6]) < 0) {
		russ_conn_fatal(conn, "error: spath is too large", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	return _hid_patch(conn);
}

int
_random_patch(struct russ_conn *conn) {
	char	new_spath[16384];
	int	hid;

	hid = (random()/(double)RAND_MAX)*hostslist.nhosts;
	if (snprintf(new_spath, sizeof(new_spath), "/hid/%d/%s", hid, &conn->req.spath[8]) < 0) {
		russ_conn_fatal(conn, "error: spath is too large", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	return _hid_patch(conn);
}

/*
* Alternate accept handler for /hid/, /host/, /next/, and /random/ .
*
* If none of the special spaths are found, the default accept
* function russ_conn_accept() is called (for further processing).
*
* @param self		connection object
* @param cfds		satisfies call requirement; ignored
* @param sfds		satisfies call requirement; ignored
* @return		0 on success; -1 on failure
*/

int
alt_russ_conn_accept(struct russ_conn *self, int *cfds, int *sfds) {
	struct russ_conn	*conn;
	struct russ_request	*req;
	int			i;

	req = &(self->req);
	if (strncmp(req->spath, "/hid/", 5) == 0) {
		_hid_patch(conn);
	} else if (strncmp(req->spath, "/host/", 6) == 0) {
		_host_patch(conn);
	} else if (strncmp(req->spath, "/next/", 6) == 0) {
		_next_patch(conn);
	} else if (strncmp(req->spath, "/random/", 8) == 0) {
		_random_patch(conn);
	} else {
		return russ_conn_accept(self, cfds, sfds);
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
	if ((strcmp(req->spath, "/hid/") == 0)
		|| (strcmp(req->spath, "/host/") == 0)
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
			russ_dprintf(outfd, "hid\nhost\nnext\nrandom\n");
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else if (strcmp(req->spath, "/hid") == 0) {
			if (hostslist.nhosts == 0) {
				russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			} else {
				for (i = 0; i < hostslist.nhosts; i++) {
					russ_dprintf(outfd, "%d\n", i);
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			}
		} else if (strcmp(req->spath, "/host") == 0) {
			if (hostslist.nhosts == 0) {
				russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			} else {
				for (i = 0; i < hostslist.nhosts; i++) {
					russ_dprintf(outfd, "%s\n", hostslist.hosts[i]);
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			}
		} else if ((strcmp(req->spath, "/net") == 0)
			|| (strcmp(req->spath, "/next") == 0)
			|| (strcmp(req->spath, "/random") == 0)) {
			//russ_conn_fatal(conn, RUSS_MSG_UNK_SERVICE, RUSS_EXIT_SUCCESS);
			russ_conn_fatal(conn, "error: unknown service", RUSS_EXIT_SUCCESS);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
	}
	exit(0);
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

void
alt_russ_listener_loop(struct russ_listener *self, russ_req_handler handler) {
	struct russ_conn	*conn;

	while (1) {
		if ((conn = russ_listener_answer(self, RUSS_TIMEOUT_NEVER)) == NULL) {
			fprintf(stderr, "error: cannot answer connection\n");
			continue;
		}

		hostslist.next = (hostslist.next+1 >= hostslist.nhosts) ? 0 : hostslist.next+1;
		random(); /* tickle */

		if (fork() == 0) {
			russ_listener_close(self);
			self = russ_listener_free(self);
			if ((russ_conn_await_request(conn) < 0)
				|| (alt_russ_conn_accept(conn, NULL, NULL) < 0)) {
				exit(-1);
			}
			handler(conn);
			russ_conn_fatal(conn, RUSS_MSG_NO_EXIT, RUSS_EXIT_SYS_FAILURE);
			exit(0);
		}
		russ_conn_close(conn);
		conn = russ_conn_free(conn);
	}
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
	for (i = 0; i < MAX_HOSTS; i++) {
		line = NULL;
		if ((nbytes = getline(&line, &line_size, f)) < 0) {
			break;
		}
		if (line[nbytes-1] == '\n') {
			line[nbytes-1] = '\0';
		}
		hostslist.hosts[i] = line;
	}
	hostslist.nhosts = i;
	hostslist.next = -1;
	return 0;
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
		exit(-1);
	}

	if (argc > 1) {
		hostsfilename = argv[1];
	}
	if ((hostsfilename != NULL) && (load_hostsfile(hostsfilename) < 0)) {
		fprintf(stderr, "error: could not load hosts file\n");
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
	alt_russ_listener_loop(lis, master_handler);
	exit(0);
}
