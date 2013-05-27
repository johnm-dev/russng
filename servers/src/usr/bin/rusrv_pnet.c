/*
** bin/rusrv_pnet.c
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
"/count\n"
"    Output the number of targets registered.\n"
"\n"
"/first/... <args>\n"
"\n"
"/host/<user@host>/... <args>\n"
"    Connect to service ... at target (i.e., user@host) verified\n"
"    by a lookup into the hostsfile list. Only available if a\n"
"    hostsfile was given at startup.\n"
"\n"
"/id/<index>/... <args>\n"
"    Connect to service ... at target identified by a lookup into\n"
"    the hostsfile list at <index>. A negative index starts at the\n"
"    last entry (1 is the last entry). An index starting with :\n"
"    loops around to continue the lookup.\n"
"\n"
"/net/... <args>\n"
"\n"
"/next/... <args>\n"
"    Connect to the 'next' target selected from the hostsfile\n"
"    list. Each call bumps to 'next' and wraps to 0 as needed.\n"
"\n"
"/random/... <args>\n"
"    Connect to a randomly selected target from the hostsfile\n"
"    list.\n";

/**
* Dial a service and splice its connection fds into the given
* connection fds. The real and effective uid/gid are also set.
*
* @param sess		session object
*/
void
redial_and_splice(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;	
	struct russ_conn	*conn2;

	/* switch user */
	if (russ_switch_user(conn->creds.uid, conn->creds.gid, 0, NULL) < 0) {
		russ_standard_answer_handler(conn);
		russ_conn_fatal(conn, RUSS_MSG_NO_SWITCH_USER, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* switch user, dial next service, and splice */
	if (((conn2 = russ_dialv(RUSS_DEADLINE_NEVER, req->op, req->spath, req->attrv, req->argv)) == NULL)
		|| (russ_conn_splice(conn, conn2) < 0)) {
		russ_conn_close(conn2);
		russ_standard_answer_handler(conn);
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
}

/**
* Handler for the / service.
*
* Provides HELP.
*
* @param sess		session object
*/
void
svc_root_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;

	switch (req->opnum) {
	case RUSS_OPNUM_HELP:
		russ_dprintf(conn->fds[1], "%s", HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
}

/**
* Handler for the /count service.
*
* Output the number of targets.
*
* @param sess		session object
*/
void
svc_count_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;

	switch (req->opnum) {
	case RUSS_OPNUM_EXECUTE:
		russ_dprintf(conn->fds[1], "%d", hostslist.nhosts);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	default:
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}
}

/**
* Handler for the /first service.
*
* Convert:
*	first/... -> <relay_addr>/<userhost>/...
* where <userhost> is select because it answers.
*
* @param sess		session object
*/
void
svc_first_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;

	/* NIY */
	exit(1);
}

/**
* Handler for the /host service.
*
* Convert:
*	host/<userhost>/... -> <relay_addr>/<userhost>/...
*
* @param sess		session object
*/
void
svc_host_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	char			*p, *spath_tail, *userhost, *relay_addr;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	int			i;

	if (russ_misc_str_count(req->spath, "/") < 3) {
		/* local */
		if (russ_standard_answer_handler(conn) < 0) {
			russ_conn_exit(conn, RUSS_EXIT_FAILURE);
		} else {
			switch (req->opnum) {
			case RUSS_OPNUM_LIST:
				if (hostslist.nhosts == 0) {
					russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
				} else {
					for (i = 0; i < hostslist.nhosts; i++) {
						russ_dprintf(conn->fds[1], "%s\n", hostslist.hosts[i]);
					}
					russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
				}
				break;
			case RUSS_OPNUM_HELP:
				svc_root_handler(sess);
				break;
			default:
				russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
			}
		}
	} else {
		/* extract and validate user@host and new_spath */
		userhost = &(req->spath[6]);
		if ((p = index(userhost, '/')) == NULL) {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			return;
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
			return;
		}

		relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
		if (snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, spath_tail) < 0) {
			russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
			return;
		}
		free(req->spath);
		req->spath = strdup(new_spath);

		redial_and_splice(sess);
	}
}

/**
* Handler for the /id service.
*
* Convert:
*	id/<index>/... -> <relay_addr>/<userhost>/...
*
* @param sess		session object
*/
void
svc_id_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	char			*p, *spath_tail, *s, *userhost, *relay_addr;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	int			i, idx, wrap = 0;

	if (russ_misc_str_count(req->spath, "/") < 3) {
		/* local */
		if (russ_standard_answer_handler(conn) < 0) {
			russ_conn_exit(conn, RUSS_EXIT_FAILURE);
		} else {
			switch (req->opnum) {
			case RUSS_OPNUM_LIST:
				if (hostslist.nhosts == 0) {
					russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
				} else {
					for (i = 0; i < hostslist.nhosts; i++) {
						russ_dprintf(conn->fds[1], "%d\n", i);
					}
					russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
				}
				break;
			case RUSS_OPNUM_HELP:
				svc_root_handler(sess);
				break;
			default:
				russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
			}
		}
	} else {
		/* extract and validate user@host and new_spath */
		s = &(req->spath[4]);
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
			return;
		}
		userhost = hostslist.hosts[idx];

		relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
		if (snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, spath_tail) < 0) {
			russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
			return;
		}
		free(req->spath);
		req->spath = strdup(new_spath);

		redial_and_splice(sess);
	}
}

/**
* Handler for the /net service.
*
* Convert:
*	net/<userhost>/... -> <relay_addr>/<userhost>/...
*
* @param sess		session object
*/
void
svc_net_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	char			*p, *spath_tail, *userhost, *relay_addr;
	char			new_spath[RUSS_REQ_SPATH_MAX];

	if (russ_misc_str_count(req->spath, "/") < 3) {
		/* local */
		if (russ_standard_answer_handler(conn) < 0) {
			russ_conn_exit(conn, RUSS_EXIT_FAILURE);
		} else {
			switch (req->opnum) {
			case RUSS_OPNUM_LIST:
				russ_conn_fatal(conn, "error: unspecified service", RUSS_EXIT_SUCCESS);
				break;
			case RUSS_OPNUM_HELP:
				svc_root_handler(sess);
				break;		
			default:
				russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
			}
		}
	} else {
		/* extract and validate user@host and new_spath */
		userhost = &(req->spath[5]);
		if ((p = index(userhost, '/')) == NULL) {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
			return;
		}
		spath_tail = strdup(p+1);
		p[0] = '\0'; /* terminate userhost */

		relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
		if (snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, spath_tail) < 0) {
			russ_conn_fatal(conn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
			return;
		}
		free(req->spath);
		req->spath = strdup(new_spath);

		redial_and_splice(sess);
	}
}

/**
* Handler for the /next service.
*
* Depends on the a "next" counter which determines the next target
* id to use.
*
* Convert:
*	next/... -> <relay_addr>/<userhost>/...
* where userhost is selected using a 'next' counter.
*
* @param sess		session object
*/
void
svc_next_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	int			idx;

	idx = hostslist.next;
	if (snprintf(new_spath, sizeof(new_spath), "/id/%d/%s", idx, &(req->spath[6])) < 0) {
		russ_conn_fatal(conn, "error: spath is too large", RUSS_EXIT_FAILURE);
		return;
	}
	free(req->spath);
	req->spath = strdup(new_spath);
	svc_id_handler(sess);
}

/**
* Handler for the /random service.
*
* Select a target id at random.
*
* Convert:
*	random/... -> <relay_addr>/<userhost>/...
* where userhost is selected at random from the list of targets.
*
* @param sess		session object
*/
void
svc_random_handler(struct russ_sess *sess) {
	struct russ_conn	*conn = sess->conn;
	struct russ_req		*req = sess->req;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	int			idx;

	idx = (random()/(double)RAND_MAX)*hostslist.nhosts;
	if (snprintf(new_spath, sizeof(new_spath), "/id/%d/%s", idx, &(req->spath[8])) < 0) {
		russ_conn_fatal(conn, "error: spath is too large", RUSS_EXIT_FAILURE);
		exit(0);
	}
	free(req->spath);
	req->spath = strdup(new_spath);
	svc_id_handler(sess);
}

struct russ_conn *
accept_handler(struct russ_lis *self, russ_deadline deadline) {
	struct russ_conn	*conn;

	if ((conn = russ_lis_accept(self, deadline)) != NULL) {
		hostslist.next = (hostslist.next+1 >= hostslist.nhosts) ? 0 : hostslist.next+1;
		random(); /* tickle */
	}
	return conn;
}

/**
* Load hosts list from file.
*
* @param filename	hosts filename
* @return		0 on success; -1 on failure
*/
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
	struct russ_svcnode	*root, *node;
	struct russ_svr		*svr;

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

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "count", svc_count_handler)) == NULL)
//		|| ((node = russ_svcnode_add(root, "first", svc_first_handler)) == NULL)
//		|| (russ_svcnode_set_virtual(node, 1) < 0)
//		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((node = russ_svcnode_add(root, "host", svc_host_handler)) == NULL)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((node = russ_svcnode_add(root, "id", svc_id_handler)) == NULL)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((node = russ_svcnode_add(root, "net", svc_net_handler)) == NULL)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((node = russ_svcnode_add(root, "next", svc_next_handler)) == NULL)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((node = russ_svcnode_add(root, "random", svc_random_handler)) == NULL)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)
		|| (russ_svr_set_accept_handler(svr, accept_handler) < 0)) {
		fprintf(stderr, "error: cannot set up\n");
		exit(1);
	}

	if (russ_svr_announce(svr,
		russ_conf_get(conf, "server", "path", NULL),
		russ_conf_getsint(conf, "server", "mode", 0600),
		russ_conf_getint(conf, "server", "uid", getuid()),
		russ_conf_getint(conf, "server", "gid", getgid())) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);	
}
