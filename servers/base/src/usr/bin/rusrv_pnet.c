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

#include <russ.h>

#define DEFAULT_DIAL_TIMEOUT	(30000)
#define DEFAULT_RELAY_ADDR	"+/ssh"
#define MAX_TARGETS		(32768)

struct target {
	char	*userhost;
	char	*cgroup;
};

struct targetslist {
	struct target	targets[MAX_TARGETS];
	int		n;
	int		next;
};

/* global */
struct russ_conf	*conf = NULL;
char			*targetsfilename = NULL;
struct targetslist	targetslist;
const char		*HELP = 
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
"    by a lookup into the targetsfile list. Only available if a\n"
"    targetsfile was given at startup.\n"
"\n"
"/id/<index>/... <args>\n"
"    Connect to service ... at target identified by a lookup into\n"
"    the targetsfile list at <index>. A negative index starts at the\n"
"    last entry (1 is the last entry). An index starting with :\n"
"    loops around to continue the lookup.\n"
"\n"
"/net/<user@host>/... <args>\n"
"    Connect to service ... at unregistered target (i.e.,\n"
"    user@host).\n"
"\n"
"/next/... <args>\n"
"    Connect to the 'next' target selected from the targetsfile\n"
"    list. Each call bumps to 'next' and wraps to 0 as needed.\n"
"\n"
"/random/... <args>\n"
"    Connect to a randomly selected target from the targetsfile\n"
"    list.\n";

/**
* Handler for the / service.
*
* Provides HELP.
*
* @param sess		session object
*/
void
svc_root_handler(struct russ_sess *sess) {
	/* auto handling in svr */
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
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		russ_dprintf(sconn->fds[1], "%d", targetslist.n);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
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
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	/* NIY */
	exit(1);
}

void
svc_host_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	int			i;

	if (req->opnum == RUSS_OPNUM_LIST) {
		if (targetslist.n == 0) {
			russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		} else {
			for (i = 0; i < targetslist.n; i++) {
				russ_dprintf(sconn->fds[1], "%s\n", targetslist.targets[i].userhost);
			}
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		}
		exit(0);
	}
}

char *
get_userhost(char *spath) {
	char	*userhost;

	userhost = russ_str_dup_comp(spath, '/', 2);
	return userhost;
}

char *
get_valid_userhost(char *spath) {
	char	*userhost;
	int	i;

	if ((userhost = get_userhost(spath)) != NULL) {
		for (i = 0; i < targetslist.n; i++) {
			if (strcmp(userhost, targetslist.targets[i].userhost) == 0) {
				return userhost;
			}
		}
		userhost = russ_free(userhost);
	}
	return userhost;
}

void
svc_host_userhost_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*userhost;

	if ((userhost = get_valid_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_LIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
	userhost = russ_free(userhost);
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
svc_host_userhost_other_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	char			*relay_addr, *tail = NULL, *userhost = NULL;
	int			i, n;

	if ((userhost = get_valid_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* set up new spath */
	tail = strchr(req->spath+1, '/');
	tail = strchr(tail+1, '/')+1;
	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (((n = snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, tail)) < 0)
		|| (n >= sizeof(new_spath))) {
		russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	userhost = russ_free(userhost);
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);

	russ_sconn_redial_and_splice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
}

void
svc_id_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	int			i;

	if (req->opnum == RUSS_OPNUM_LIST) {
		if (targetslist.n == 0) {
			russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		} else {
			for (i = 0; i < targetslist.n; i++) {
				russ_dprintf(sconn->fds[1], "%d\n", i);
			}
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		}
		exit(0);
	}
}

int
get_valid_id_index(char *spath, int *idx, int *wrap) {
	char	buf[64];

	if (russ_str_get_comp(spath, '/', 2, buf, sizeof(buf)) < 0) {
		return -1;
	}
	if (sscanf(buf, ":%d", idx) == 1) {
		*wrap = 1;
	} else if (sscanf(buf, "%d", idx) == 1) {
		*wrap = 0;
	} else {
		return -1;
	}
	if ((*idx < 0) || (*idx >= targetslist.n)) {
		return -1;
	}
	return 0;
}

void
svc_id_index_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	int			i, idx, wrap;

	if (get_valid_id_index(req->spath, &idx, &wrap) < 0) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (req->opnum ==  RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_LIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

/**
* Handler for the /id/<index>/... service.
*
* Convert:
*	id/<index>/... -> <relay_addr>/<userhost>/...
*
* @param sess		session object
*/
void
svc_id_index_other_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	char			*relay_addr, *tail = NULL, *userhost = NULL;
	int			i, idx, n, wrap = 0;

	if (get_valid_id_index(req->spath, &idx, &wrap) < 0) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* wrap if requested; handle negative indexes */
	if (wrap) {
		idx = idx % targetslist.n;
	}
	if ((idx < 0) && (-idx <= targetslist.n)) {
		idx = targetslist.n+idx;
	}

	/* set up new spath */
	tail = strchr(req->spath+1, '/');
	tail = strchr(tail+1, '/')+1;
	userhost = targetslist.targets[idx].userhost;
	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (((n = snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, tail)) < 0)
		|| (n >= sizeof(new_spath))) {
		russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	userhost = russ_free(userhost);
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);

	russ_sconn_redial_and_splice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
}

void
svc_net_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_LIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_net_userhost_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			*userhost;

	if ((userhost = get_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_LIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
	userhost = russ_free(userhost);
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
svc_net_userhost_other_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	char			*relay_addr, *tail = NULL, *userhost = NULL;
	int			n;

	if ((userhost = get_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* set up new spath */
	tail = strchr(req->spath+1, '/');
	tail = strchr(tail+1, '/')+1;
	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (((n = snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, tail)) < 0)
		|| (n >= sizeof(new_spath))) {
		russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	userhost = russ_free(userhost);
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);
	russ_sconn_redial_and_splice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
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
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	int			idx, n;

	idx = targetslist.next;
	if (((n = snprintf(new_spath, sizeof(new_spath), "/id/%d/%s", idx, &(req->spath[6]))) < 0)
		|| (n >= sizeof(new_spath))) {
		russ_sconn_fatal(sconn, "error: spath is too large", RUSS_EXIT_FAILURE);
		exit(0);
	}
	req->spath = russ_free(req->spath);
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
	struct russ_sconn	*sconn = sess->sconn;
	struct russ_req		*req = sess->req;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	int			idx, n;

	idx = (random()/(double)RAND_MAX)*targetslist.n;
	if (((n = snprintf(new_spath, sizeof(new_spath), "/id/%d/%s", idx, &(req->spath[8]))) < 0)
		|| (n >= sizeof(new_spath))) {
		russ_sconn_fatal(sconn, "error: spath is too large", RUSS_EXIT_FAILURE);
		exit(0);
	}
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);
	svc_id_handler(sess);
}

struct russ_sconn *
accept_handler(struct russ_lis *self, russ_deadline deadline) {
	struct russ_sconn	*sconn;

	if ((sconn = russ_lis_accept(self, deadline)) != NULL) {
		targetslist.next = (targetslist.next+1 >= targetslist.n) ? 0 : targetslist.next+1;
		random(); /* tickle */
	}
	return sconn;
}

/**
* Load targets list from file.
*
* @param filename	targets filename
* @return		0 on success; -1 on failure
*/
int
load_targetsfile(char *filename) {
	char	*line, *p;
	int	i;
	size_t	line_size;
	ssize_t	nbytes;
	FILE	*f;

	if ((f = fopen(filename, "r")) == NULL) {
		return -1;
	}
	for (i = 0, targetslist.n = 0; i < MAX_TARGETS; i++) {
		line = NULL;
		if ((nbytes = getline(&line, &line_size, f)) < 0) {
			break;
		}
		if (line[nbytes-1] == '\n') {
			line[nbytes-1] = '\0';
		}
		if ((line[0] == '\0') || (line[0] == '#')) {
			/* ignore empty and comment lines */
			line = russ_free(line);
			continue;
		}
		for (p = line; (!isblank(*p)) && (*p != '\0'); p++);
		if (*p != '\0') {
			*p = '\0';
			for (p++; isblank(*p); p++);
			if ((p != NULL) && ((p = strdup(p)) == NULL)) {
				/* fatal: OOM */
				return -1;
			}
		}
		targetslist.targets[i].userhost = line;
		targetslist.targets[i].cgroup = p;
		targetslist.n++;
	}
	targetslist.next = -1;
	return 0;
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_pnet [<conf options>] -- <targetsfile>\n"
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
	targetslist.n = 0;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((argc < 2) || ((conf = russ_conf_init(&argc, argv)) == NULL)) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	if (argc < 2) {
		fprintf(stderr, "error: missing targets file\n");
		exit(1);
	}
	targetsfilename = argv[1];
	if (load_targetsfile(targetsfilename) < 0) {
		fprintf(stderr, "error: could not load targets file\n");
		exit(1);
	}

	if (((root = russ_svcnode_new("", svc_root_handler)) == NULL)
		|| ((node = russ_svcnode_add(root, "count", svc_count_handler)) == NULL)
//		|| ((node = russ_svcnode_add(root, "first", svc_first_handler)) == NULL)
//		|| (russ_svcnode_set_virtual(node, 1) < 0)
//		|| (russ_svcnode_set_auto_answer(node, 0) < 0)

		|| ((node = russ_svcnode_add(root, "host", svc_host_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_host_userhost_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_host_userhost_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)

		|| ((node = russ_svcnode_add(root, "id", svc_id_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_id_index_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_id_index_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)

		|| ((node = russ_svcnode_add(root, "net", svc_net_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_net_userhost_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_net_userhost_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)

		|| ((node = russ_svcnode_add(root, "next", svc_next_handler)) == NULL)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((node = russ_svcnode_add(root, "random", svc_random_handler)) == NULL)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_auto_answer(node, 0) < 0)
		|| ((svr = russ_svr_new(root, RUSS_SVR_TYPE_FORK)) == NULL)
		|| (russ_svr_set_accepthandler(svr, accept_handler) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)) {
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
