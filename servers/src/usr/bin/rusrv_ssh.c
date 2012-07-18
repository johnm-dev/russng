/*
** bin/rusrv_ssh.c
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

#include "configparser.h"
#include "russ.h"

#define MAX_HOSTS	(1024)

struct hostslist {
	char	*hosts[MAX_HOSTS];
	int	nhosts;
	int	next;
};

/* global */
struct configparser	*config = NULL;
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
"net/user@host/... <args>\n"
"    Connect to service ... at user@host using ssh.\n"
"\n"
"next/... <args>\n"
"    Connect to the 'next' host selected from the hostsfile list.\n"
"    Each call bumps to 'next' and wraps to 0 as needed.\n"
"\n"
"random/... <args>\n"
"    Connect to a randomly select host selected from the hostsfile\n"
"    list.\n";

#define SSH_EXEC	"/usr/bin/ssh"
#define RUDIAL_EXEC	"/usr/bin/rudial"

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

void
execute(struct russ_conn *conn, char *userhost, char *new_spath) {
	char	*args[1024];
	int	nargs;
	int	i, status, pid;

	switch_user(conn);

	/* build args array */
	nargs = 0;
	args[nargs++] = SSH_EXEC;
	args[nargs++] = userhost;
	args[nargs++] = RUDIAL_EXEC;
	if ((conn->req.attrv != NULL) && (conn->req.attrv[0] != NULL)) {
		for (i = 0; conn->req.attrv[i] != NULL; i++) {
			args[nargs++] = "-a";
			args[nargs++] = conn->req.attrv[i];
		}
	}
	args[nargs++] = conn->req.op;
	args[nargs++] = new_spath;
	if ((conn->req.argv != NULL) && (conn->req.argv[0] != NULL)) {
		for (i = 0; conn->req.argv[i] != NULL; i++) {
			args[nargs++] = conn->req.argv[i];
		}
	}
	args[nargs++] = NULL;

	/* fix up fds and exec */
	signal(SIGCHLD, SIG_DFL);
	if ((pid = fork()) == 0) {
		dup2(conn->fds[0], 0);
		dup2(conn->fds[1], 1);
		dup2(conn->fds[2], 2);
		close(conn->fds[3]);
		execv(args[0], args);

		/* should not get here! */
		russ_dprintf(conn->fds[2], "error: could not execute\n");
		exit(-1);
	}
	close(conn->fds[0]);
	close(conn->fds[1]);
	close(conn->fds[2]);
	waitpid(pid, &status, 0);

	russ_conn_exit(conn, WEXITSTATUS(status));
	russ_conn_close(conn);
	exit(0);
}

void
svc_hid_handler(struct russ_conn *conn) {
	char	*p, *new_spath, *hids, *userhost;
	int	i, hid;

	/* extract and validate user@host and new_spath */
	hids = &conn->req.spath[5];
	if ((p = index(hids, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	new_spath = strdup(p);
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

	execute(conn, userhost, new_spath);
}

void
svc_host_handler(struct russ_conn *conn) {
	char	*p, *new_spath, *userhost;
	int	i;

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[6];
	if ((p = index(userhost, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	for (i = 0; i < hostslist.nhosts; i++) {
		if (strcmp(userhost, hostslist.hosts[i]) == 0) {
			break;
		}
	}
	new_spath = strdup(p);
	p[0] = '\0'; /* terminate userhost */
	if (i == hostslist.nhosts) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	execute(conn, userhost, new_spath);
}

void
svc_net_handler(struct russ_conn *conn) {
	char	*p, *new_spath, *userhost;
	int	i;

	/* extract and validate user@host and new_spath */
	userhost = &conn->req.spath[5];
	if ((p = index(userhost, '/')) == NULL) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}
	new_spath = strdup(p);
	p[0] = '\0'; /* terminate userhost */

	execute(conn, userhost, new_spath);
}

void
svc_next_handler(struct russ_conn *conn) {
	char	new_spath[16384];
	int	hid;

	hid = hostslist.next;
	sprintf(new_spath, "/hid/%d/%s", hid, &conn->req.spath[6]);
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	svc_hid_handler(conn);
}

void
svc_random_handler(struct russ_conn *conn) {
	char	new_spath[16384];
	int	hid;

	hid = int((random()/(double)RAND_MAX)*hostslist.nhosts);
	sprintf(new_spath, "/hid/%d/%s", hid, &conn->req.spath[6]);
	free(conn->req.spath);
	conn->req.spath = strdup(new_spath);
	svc_hid_handler(conn);
}

/*
* All ops are passed to handlers for /hid/*, /host/*, /net/*,
* /next/*, and /random/* spaths.
*/
void
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	int			i;

	req = &(conn->req);
	if (strncmp(req->spath, "/hid/", 5) == 0) {
		svc_hid_handler(conn);
	} else if (strncmp(req->spath, "/host/", 6) == 0) {
		svc_host_handler(conn);
	} else if (strncmp(req->spath, "/next/", 6) == 0) {
		svc_next_handler(conn);
	} else if (strncmp(req->spath, "/net/", 5) == 0) {
		svc_net_handler(conn);
	} else if (strncmp(req->spath, "/random/", 8) == 0) {
		svc_random_handler(conn);
	} else if (strcmp(req->op, "execute") == 0) {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	} else if (strcmp(req->op, "help") == 0) {
        	russ_dprintf(conn->fds[1], "%s", HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else if (strcmp(req->op, "list") == 0) {
		if (strcmp(req->spath, "/") == 0) {
			russ_dprintf(conn->fds[1], "hid\nhost\nnet\n");
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else if (strcmp(req->spath, "/hid") == 0) {
			if (hostslist.nhosts == 0) {
				russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			} else {
				for (i = 0; i < hostslist.nhosts; i++) {
					russ_dprintf(conn->fds[1], "%d\n", i);
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			}
		} else if (strcmp(req->spath, "/host") == 0) {
			if (hostslist.nhosts == 0) {
				russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			} else {
				for (i = 0; i < hostslist.nhosts; i++) {
					russ_dprintf(conn->fds[1], "%s\n", hostslist.hosts[i]);
				}
				russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
			}
		} else if (strcmp(req->spath, "/net") == 0) {
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
"usage: rusrv_ssh <conf>\n"
"                 [options]\n"
"\n"
"russ-based server for ssh-based remote connections. Configuration\n"
"can be obtained from the conf file if no options are used, otherwise\n"
"all configuration is taken from the given options.\n"
"\n"
"Options:"
"-c <conf>\n"
"    Use conf file for server configuration.\n"
"-h <hostsfile>\n"
"    Set up /hosts/* and /hid/* services.\n"
"-p <path>\n"
"    Use path for service address.\n"
);
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
	hostslist.next = i-1;
	return 0;
}

void
alt_russ_listener_loop(struct russ_listener *self, russ_req_handler handler) {
	struct russ_conn	*conn;

	while (1) {
		if ((conn = russ_listener_answer(self, RUSS_TIMEOUT_NEVER)) == NULL) {
			fprintf(stderr, "error: cannot answer connection\n");
			continue;
		}
		if (hostslist.nhosts > 0) {
			hostslist.next = (hostslist.next+1 >= hostslist.nhosts) ? 0 : hostslist.next+1;
		}
		if (fork() == 0) {
			russ_listener_close(self);
			self = russ_listener_free(self);
			if ((russ_conn_await_request(conn) < 0)
				|| (russ_conn_accept(conn, NULL, NULL) < 0)) {
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
main(int argc, char **argv) {
	struct russ_listener	*lis;
	char			*filename, *path;
	int			mode, uid, gid;
	char			*arg;
	int			i;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	srandom(time(NULL));
	hostslist.nhosts = 0;

	if (argc < 2) {
		print_usage(argv);
		exit(-1);
	}
	if (argv[1][0] == '-') {
		/* options */
		mode = 0666;
		uid = getuid();
		gid = getgid();
		path = NULL;

		i = 1;
		while (i < argc) {
			arg = argv[i++];

			if ((strcmp(arg, "-h") == 0) && (i < argc)) {
				hostsfilename = argv[i++];
			} else if ((strcmp(arg, "-p") == 0) && (i < argc)) {
				path = argv[i++];
			} else {
				fprintf(stderr, "error: bad/missing arguments");
				exit(-1);
			}
		}
	} else if (argc == 2) {
		/* configuration file */
		filename = argv[1];
		if ((config = configparser_read(filename)) == NULL) {
			fprintf(stderr, "error: could not read config file\n");
			exit(-1);
		}

		mode = configparser_getsint(config, "server", "mode", 0600);
		uid = configparser_getint(config, "server", "uid", getuid());
		gid = configparser_getint(config, "server", "gid", getgid());
		path = configparser_get(config, "server", "path", NULL);
	}

	if ((hostsfilename != NULL) && (load_hostsfile(hostsfilename) < 0)) {
		fprintf(stderr, "error: could not load hosts file\n");
		exit(-1);
	}

	if ((lis = russ_announce(path, mode, uid, gid)) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	alt_russ_listener_loop(lis, master_handler);
}
