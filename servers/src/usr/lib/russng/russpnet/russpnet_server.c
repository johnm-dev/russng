/*
** lib/russng/russpnet_server.c
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

#include <ctype.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#include <russ/russ.h>

#define DEFAULT_DIAL_TIMEOUT	(30000)
#define DEFAULT_RELAY_ADDR	"+/ssh"
#define MAX_TARGETS		(32768)

struct target {
	char	*userhost;
	char	*cgroup;
};

struct targetslist {
	struct russ_conf	*targetconfs[MAX_TARGETS];
	int			n;
};

/* global */
struct russ_conf	*conf = NULL;
struct russ_conf	*targetsconf = NULL;
char			*targetsfilename = NULL;
struct targetslist	targetslist;
char			fqlocalhostname[1024] = "";

const char		*HELP = 
"Provides access to local/remote targets (e.g., user@host) using a\n"
"relay (e.g., ssh service).\n"
"\n"
"/count\n"
"    Output the number of targets registered.\n"
"\n"
"/gettargets\n"
"    Get the targets file information (configuration file format).\n"
"\n"
"/host/<user@host>/... <args>\n"
"    Connect to service ... at target (i.e., user@host) verified\n"
"    by a lookup into the targetsfile list. Only available if a\n"
"    targetsfile was given at startup.\n"
"\n"
"/id/<index>/... <args>\n"
"    Connect to service ... at target (host only) identified by a\n"
"    lookup into the targetsfile list at <index>. A negative index\n"
"    starts at the last entry (-1 is the last entry). An index\n"
"    starting with : loops around to continue the lookup.\n"
"\n"
"/net/<user@host>/... <args>\n"
"    Connect to service ... at unregistered target (i.e.,\n"
"    user@host).\n"
"\n"
"/run/<index>/<method> <args>\n"
"    Run exec-based service, where <method> corresponds to what is\n"
"    provided by the exec server (e.g., noshell, shell, login), at\n"
"    target identified by a lookup into the targetsfile list at\n"
"    <index>. A negative index starts at the last entry (1 is the\n"
"    last entry). An index starting with : loops around to continue\n"
"    the lookup. If a cgroup is defined in targetsfile, it is used\n"
"    in the call.\n";

/**
* Check if given hostname resolves to the local host.
*
* @param hostname	name to check
* @return		1 for match; 0 for no match
*/
int
is_localhost(char *hostname) {
	struct hostent	*hent = NULL;

	if (((hent = gethostbyname(hostname)) != NULL)
		&& (strcmp(fqlocalhostname, hent->h_name) == 0)) {
		return 1;
	}
	return 0;
}

/**
* Set the fqlocalhostname to the fq hostname for the local host.
*/
void
set_fqlocalhostname(void) {
	struct hostent	*hent = NULL;

	fqlocalhostname[sizeof(fqlocalhostname)-1] = '\0';
	if ((gethostname(fqlocalhostname, sizeof(fqlocalhostname)-1) < 0)
		|| ((hent = gethostbyname(fqlocalhostname)) == NULL)
		|| (strlen(hent->h_name) >= sizeof(fqlocalhostname))) {
		fqlocalhostname[0] = '\0';
	} else {
		strncpy(fqlocalhostname, hent->h_name, sizeof(fqlocalhostname));
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
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		russ_dprintf(sconn->fds[1], "%d", targetslist.n);
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

/**
* Handler for the /gettargets service.
*
* Output the targets configuration.
*
* @param sess		session object
*/
void
svc_gettargets_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_EXECUTE) {
		if (russ_conf_writefd(targetsconf, sconn->fds[1]) < 0) {
			russ_sconn_fatal(sconn, "error: failed do output targets info", RUSS_EXIT_FAILURE);
		} else {
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		}
		exit(0);
	}
}

void
svc_host_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	struct russ_conf	*targetconf = NULL;
	char			*ref;
	int			i;

	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		if (targetslist.n == 0) {
			russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		} else {
			for (i = 0; i < targetslist.n; i++) {
				if (((targetconf = targetslist.targetconfs[i]) == NULL)
					|| ((ref = russ_conf_getref(targetconf, "target", "userhost")) == NULL)) {
					continue;
				}
				russ_dprintf(sconn->fds[1], "%s\n", ref);
			}
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		}
		exit(0);
	}
}

char *
get_userhost(char *spath) {
	char	*userhost = NULL;

	userhost = russ_str_dup_comp(spath, '/', 2);
	return userhost;
}

char *
get_valid_userhost(char *spath) {
	struct russ_conf	*targetconf = NULL;
	char			*ref = NULL;
	char			*userhost = NULL;
	int			i;

	if ((userhost = get_userhost(spath)) != NULL) {
		for (i = 0; i < targetslist.n; i++) {
			if (((targetconf = targetslist.targetconfs[i]) == NULL)
				|| ((ref = russ_conf_getref(targetconf, "target", "userhost")) == NULL)) {
				continue;
			}
			if (strcmp(userhost, ref) == 0) {
				return userhost;
			}
		}
		userhost = russ_free(userhost);
	}
	return userhost;
}

void
svc_host_userhost_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*userhost = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if ((userhost = get_valid_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
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
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	char			*relay_addr = NULL, *tail = NULL, *userhost = NULL;
	int			i, n;

	sconn = sess->sconn;
	req = sess->req;

	if ((userhost = get_valid_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* set up new spath */
	tail = strchr(req->spath+1, '/');
	tail = strchr(tail+1, '/')+1;
	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (russ_snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, tail) < 0) {
		russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	userhost = russ_free(userhost);
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);

	russ_sconn_redialandsplice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
}

void
svc_id_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	int			i;

	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		if (targetslist.n == 0) {
			russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
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
get_valid_id_index(char *spath, int *idx, int *oidx, int *wrap, int ntargets) {
	char	buf[64];

	*idx = -1;
	*oidx = -1;
	*wrap = -1;

	if (ntargets < 1) {
		return -1;
	}
	if (russ_str_get_comp(spath, '/', 2, buf, sizeof(buf)) < 0) {
		return -1;
	}

	if (sscanf(buf, ":%d", oidx) == 1) {
		*wrap = 1;
	} else if (sscanf(buf, "%d", oidx) == 1) {
		*wrap = 0;
	} else {
		return -1;
	}
	*idx = *oidx;

	/* negative index, wrapping, out of range */
	if (*idx < 0) {
		*idx += ntargets;
	}
	if (*wrap) {
		*idx = *idx % ntargets;
	}
	if ((*idx < 0) || (*idx >= ntargets)) {
		return -1;
	}

	return 0;
}

void
svc_id_index_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	int			i, idx, oidx, wrap;

	sconn = sess->sconn;
	req = sess->req;

	if (get_valid_id_index(req->spath, &idx, &oidx, &wrap, targetslist.n) < 0) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (req->opnum ==  RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

/**
* Handler for the /run/<index>/... service.
*
* Convert:
*	id/<index>/... -> <relay_addr>/<userhost>/...
*
* @param sess		session object
*/
void
svc_id_index_other_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	struct russ_conf	*targetconf = NULL;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	char			*relay_addr = NULL, *tail = NULL, *userhost = NULL;
	int			i, idx, n, oidx, wrap = 0;

	sconn = sess->sconn;
	req = sess->req;

	if (get_valid_id_index(req->spath, &idx, &oidx, &wrap, targetslist.n) < 0) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* set up new spath */
	tail = strchr(req->spath+1, '/');
	tail = strchr(tail+1, '/')+1;
	targetconf = targetslist.targetconfs[idx];
	userhost = russ_conf_get(targetconf, "target", "userhost", NULL);
	if ((index(userhost, '@') == NULL) && (is_localhost(userhost))) {
		if (russ_snprintf(new_spath, sizeof(new_spath), "%s", tail) < 0) {
			russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
			exit(0);
		}
	} else {
		relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
		if (russ_snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, tail) < 0) {
			russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
			exit(0);
		}
		relay_addr = russ_free(relay_addr);
	}
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);

	russ_sconn_redialandsplice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
}

void
svc_net_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
		exit(0);
	}
}

void
svc_net_userhost_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			*userhost = NULL;

	sconn = sess->sconn;
	req = sess->req;

	if ((userhost = get_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	if (req->opnum == RUSS_OPNUM_LIST) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
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
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	char			*relay_addr = NULL, *tail = NULL, *userhost = NULL;
	int			n;

	sconn = sess->sconn;
	req = sess->req;

	if ((userhost = get_userhost(req->spath)) == NULL) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* set up new spath */
	tail = strchr(req->spath+1, '/');
	tail = strchr(tail+1, '/')+1;
	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	if (russ_snprintf(new_spath, sizeof(new_spath), "%s/%s/%s", relay_addr, userhost, tail) < 0) {
		russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	userhost = russ_free(userhost);
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);
	russ_sconn_redialandsplice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
}

/**
* Handler for the /run/<index>/... service.
*
* ... is expected to be supported by the +/exec server, e.g., an exec method.
*
* Convert:
*	run/<index>/... -> <relay_addr>/<userhost>/+/exec/cgroup/<cgname>/...
*
* @param sess		session object
*/
void
svc_run_index_other_handler(struct russ_sess *sess) {
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	struct russ_conf	*targetconf = NULL;
	char			new_spath[RUSS_REQ_SPATH_MAX];
	char			*relay_addr = NULL, *tail = NULL;
	char			*userhost = NULL, *cgname = NULL, *exec_spath = NULL;
	int			i, idx, n, oidx, wrap = 0;

	sconn = sess->sconn;
	req = sess->req;

	if (get_valid_id_index(req->spath, &idx, &oidx, &wrap, targetslist.n) < 0) {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		exit(0);
	}

	/* set up new spath */
	tail = strchr(req->spath+1, '/');
	tail = strchr(tail+1, '/')+1;
	relay_addr = russ_conf_get(conf, "net", "relay_addr", DEFAULT_RELAY_ADDR);
	exec_spath = "+/exec";
	targetconf = targetslist.targetconfs[idx];
	userhost = russ_conf_get(targetconf, "target", "userhost", NULL);
	cgname = russ_conf_get(targetconf, "target", "cgroup", NULL);
	russ_str_replace_char(cgname, '/', ':');

	if ((cgname == NULL) || (strcmp(cgname, "") == 0)) {
		n = russ_snprintf(new_spath, sizeof(new_spath), "%s/%s/%s/%s", relay_addr, userhost, exec_spath, tail);
	} else {
		n = russ_snprintf(new_spath, sizeof(new_spath), "%s/%s/%s/cgroup/%s/%s", relay_addr, userhost, exec_spath, cgname, tail);
	}
	if (n < 0) {
		russ_sconn_fatal(sconn, "error: cannot patch spath", RUSS_EXIT_FAILURE);
		exit(0);
	}
	req->spath = russ_free(req->spath);
	req->spath = strdup(new_spath);

	russ_sconn_redialandsplice(sconn, russ_to_deadline(DEFAULT_DIAL_TIMEOUT), req);
}

/**
* Copy targetsconf info to targetslist.targetconfs.
*
* @return       0 on success; -1 on failure
*/
int
load_targetslist(void) {
    struct russ_conf    *targetconf = NULL;
    char            secname[128];
    int         i;

    targetslist.n = 0;

    for (i = 0; i < MAX_TARGETS; i++) {
        if ((russ_snprintf(secname, sizeof(secname), "target.%d", i) < 0)
            || ((targetconf = russ_conf_new()) == NULL)
            || (russ_conf_update_section(targetconf, "target", targetsconf, secname) < 0)) {
            targetconf = russ_free(targetconf);
            return -1;
        }
        if (!russ_conf_has_option(targetconf, "target", "id")) {
            targetconf = russ_free(targetconf);
            break;
        }
        targetslist.targetconfs[i] = targetconf;
        targetslist.n++;

    }
    return 0;
}

/**
* Load conf-style targetsfile.
*
* targetlist:
* * targetslist.targetconfs holds all target conf info
* * each targetconf has a single section named "target"
* * each target holds: id, userhost, cgroup
* 
* targetsconf:
* * all targets have info like targetlist
* * there is a "target.<idx>" section for each target
*
* @param filename	targets filename
* @return		0 on success; -1 on failure
*/
int
load_targetsfile(char *filename) {
	if ((russ_conf_read(targetsconf, filename) < 0)
		|| (load_targetslist() < 0)) {
		return -1;
	}
	return 0;
}

/**
* Load legacy format targets list from file.
*
* Both targetsconf and targetslist are set up.
*
* See load_targetsfile().
*
* @param filename	targets filename
* @return		0 on success; -1 on failure
*/
int
load_targetsfile_legacy(char *filename) {
	struct russ_conf	*targetconf = NULL;
	FILE			*f = NULL;
	size_t			line_size;
	ssize_t			nbytes;
	char			targetid[128];
	char			secname[128];
	char			*line = NULL, *p = NULL;
	int			i;

	if ((f = fopen(filename, "r")) == NULL) {
		return -1;
	}

	/* initialize */
	targetslist.n = 0;
	for (i = 0; i < MAX_TARGETS; i++) {
		targetslist.targetconfs[i] = NULL;
	}

	/* set */
	for (i = 0; i < MAX_TARGETS; i++) {
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

		/* add target conf
		*
		* note: updating a targetsconf section from a targetconf
		* is more efficient than updating targetsconf for each item.
		*/
		if ((russ_snprintf(secname, sizeof(secname), "target.%d", i) < 0)
			|| (russ_snprintf(targetid, sizeof(targetid), "%d", i) < 0)
			|| ((targetconf = russ_conf_new()) == NULL)) {
			return -1;
		}
		if ((russ_conf_set2(targetconf, "target", "id", targetid) < 0)
			|| (russ_conf_set2(targetconf, "target", "userhost", line) < 0)
			|| (russ_conf_set2(targetconf, "target", "cgroup", p) < 0)
			|| (russ_conf_update_section(targetsconf, secname, targetconf, "target") < 0)) {
			return -1;
		}
		targetslist.targetconfs[i] = targetconf;
		targetslist.n++;
	}
	return 0;
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: russpnet_server [<conf options>]\n"
"\n"
"Routes connections over the network to a fixed set of targets\n"
"identified by index or hostname.\n"
"\n"
"The targets file is set in the newtargets:filename configuration\n"
"setting or the targets:filename setting for legacy target files.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_svcnode	*node = NULL;
	struct russ_svr		*svr = NULL;
	char			*targetsfilename = NULL;

	signal(SIGPIPE, SIG_IGN);

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv);
		exit(0);
	} else if ((conf = russ_conf_load(&argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot configure\n");
		exit(1);
	}

	targetslist.n = 0;
	targetsconf = russ_conf_new();

	if (russ_conf_has_option(conf, "newtargets", "filename")) {
		if (((targetsfilename = russ_conf_get(conf, "newtargets", "filename", NULL)) == NULL)
			|| (load_targetsfile(targetsfilename) < 0)) {
			fprintf(stderr, "error: missing or bad targets file\n");
			exit(1);
		}
	} else if (russ_conf_has_option(conf, "targets", "filename")) {
		if (((targetsfilename = russ_conf_get(conf, "targets", "filename", NULL)) == NULL)
			|| (load_targetsfile_legacy(targetsfilename) < 0)) {
			fprintf(stderr, "error: missing or bad targets file\n");
			exit(1);
		}
	} else {
		fprintf(stderr, "error: targets filename not found\n");
		exit(1);
	}

	if (russ_conf_getint(conf, "targets", "fastlocalhost", 0) == 1) {
		set_fqlocalhostname();
	}

	if (((svr = russ_init(conf)) == NULL)
		|| (russ_svr_set_type(svr, RUSS_SVR_TYPE_FORK) < 0)
		|| (russ_svr_set_autoswitchuser(svr, 1) < 0)
		|| (russ_svr_set_matchclientuser(svr, 1) < 0)
		|| (russ_svr_set_help(svr, HELP) < 0)

		|| ((node = russ_svcnode_add(svr->root, "count", svc_count_handler)) == NULL)
		|| ((node = russ_svcnode_add(svr->root, "gettargets", svc_gettargets_handler)) == NULL)

		|| ((node = russ_svcnode_add(svr->root, "host", svc_host_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_host_userhost_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_host_userhost_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_autoanswer(node, 0) < 0)

		|| ((node = russ_svcnode_add(svr->root, "id", svc_id_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_id_index_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_id_index_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_autoanswer(node, 0) < 0)

		|| ((node = russ_svcnode_add(svr->root, "net", svc_net_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_net_userhost_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_net_userhost_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_autoanswer(node, 0) < 0)

		/* use svc_id_*_handlers as appropriate */
		|| ((node = russ_svcnode_add(svr->root, "run", svc_id_handler)) == NULL)
		|| ((node = russ_svcnode_add(node, "*", svc_id_index_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| ((node = russ_svcnode_add(node, "*", svc_run_index_other_handler)) == NULL)
		|| (russ_svcnode_set_wildcard(node, 1) < 0)
		|| (russ_svcnode_set_virtual(node, 1) < 0)
		|| (russ_svcnode_set_autoanswer(node, 0) < 0)) {

		fprintf(stderr, "error: cannot set up server\n");
		exit(1);
	}
	russ_svr_loop(svr);
	exit(0);	
}
