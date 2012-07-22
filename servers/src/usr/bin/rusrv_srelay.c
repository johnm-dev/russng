/*
** bin/rusrv_srelay.c
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
#include <memory.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_configparser.h"
#include "russ_priv.h"
#include "disp.h"

struct russ_configparser	*config;

char	*HELP =
"Relay service (forwards bytes between local and remote).\n"
"\n"
"Hosts are organized under a locally defined cluster name so that\n"
"dialing a service on another host requires an address like\n"
"+relay/net/myhost.com/debug (assuming +relay is the local\n"
"service address for the relay itself.\n"
"\n"
"/debug\n"
"    Outputs russ connection information.\n"
"\n"
"/dial/<cluster>/<hostname>/...\n"
"    Dial service ... at <hostname> belonging to <cluster>.\n"
"    Supported cluster names can be determined by 'ruls /dial'.\n"
"    However, because hostnames belonging to a cluster are often\n"
"    matched, there is usually no canonical list of hosts that\n"
"    comprise a cluster. Nevertheless, doing a\n"
"    'ruls /dial/<cluster>' may return a list in some cases.\n";

/**
* Service handler for /debug .
*
* @param conn		connection object
*/
void
svc_debug_handler(struct russ_conn *conn) {
	russ_dprintf(conn->fds[1], "nothing implemented yet\n");
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

/**
* Service handler for /dial .
*
* @param conn		connection object
*/
void
svc_dial_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	char			**section_names, **p;

	req = &(conn->req);
	if (strcmp(req->op, "list") == 0) {
		if ((section_names = russ_configparser_sections(config)) == NULL) {
			russ_conn_exit(conn, RUSS_EXIT_FAILURE);
			return;
		}
		for (p = section_names; *p != NULL; p++) {
			if (strncmp(*p, "cluster.", 8) == 0) {
				russ_dprintf(conn->fds[1], "%s\n", (*p)+8);
			}
		}
		russ_configparser_sarray0_free(section_names);
	}
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
}

/**
* Forward bytes between client and server.
*
* @param conn		connection object
* @param sd		socket descriptor
* @return		0 on success; -1 on failure (of setup)
*/
int
forward_bytes(struct russ_conn *conn, int sd) {
	struct dispatcher	*disp;
	struct rw		*rw;
	int			i, ev;

	if ((disp = dispatcher_new(4, sd)) == NULL) {
		return -1;
	}

	if ((dispatcher_add_rw(disp, rw_new(DISPATCHER_READER, conn->fds[0])) < 0)
		|| (dispatcher_add_rw(disp, rw_new(DISPATCHER_WRITER, conn->fds[1])) < 0)
		|| (dispatcher_add_rw(disp, rw_new(DISPATCHER_WRITER, conn->fds[2])) < 0)
		|| (dispatcher_add_rw(disp, rw_new(DISPATCHER_WRITER, conn->exit_fd)) < 0)) {
		ev = -1;
		goto cleanup;
	}
	dispatcher_loop(disp);
	ev = 0;
cleanup:
	for (i = 0; i < disp->nrws; i++) {
		disp->rws[i] = rw_destroy(disp->rws[i]);
	}
	dispatcher_destroy(disp);
	return ev;
}

/**
* Connect to remote server.
*
* @param hostname	remote host
* @param port		remote port
* @return		socket descriptor
*/
int
connect_remote(char *hostname, int port) {
	struct sockaddr_in	servaddr;
	struct hostent		*hent;
	int			sd;

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
	}
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	if ((hent = gethostbyname(hostname)) == NULL) {
		return -1;
	}
	servaddr.sin_addr.s_addr = *((unsigned long *)hent->h_addr_list[0]);
	if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		return -1;
	}
	return sd;
}

/**
* Encode dial information into buffer.
*
* @param conn		connection object
* @param buf		buffer
* @param buf_size	size of buffer
* @return		# of bytes encoded; -1 on failure
*/
int
enc_dial_info(struct russ_conn *conn, char *new_spath, char *buf, int buf_size) {
	struct russ_request	*req;
	char			*bp, *bend;

	req = &(conn->req);
	bp = buf;
	bend = buf+buf_size;
	if (((bp = russ_enc_I(bp, bend, 0)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, req->op)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, new_spath)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->attrv)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->argv)) == NULL)) {
		return -1;
	}
	/* patch size */
fprintf(stderr, "dial psize (%d)\n", (int)(bp-buf-4));
	russ_enc_I(buf, bend, bp-buf-4);
	return bp-buf;
}

/**
* Dial remote host.
*
* @param conn			connection object
* @param new_spath		spath to pass to remote
* @param section_name		config section name with info
* @param cluster_name		cluster name taken from spath
* @param hostname		host name taken from spath
* @retrun			0 on success; -1 on failure
*/
static int
__dial_remote(struct russ_conn *conn, char *new_spath, char *section_name, char *cluster_name, char *hostname) {
	char	*buf;
	int	buf_size;
	int	port;
	int	sd = -1;
	int	n;

	/* prep */
	if ((conn->cred.gid == 0)
		|| (conn->cred.uid == 0)
		|| (setgid(conn->cred.gid) < 0)
		|| (setuid(conn->cred.uid) < 0)) {
		fprintf(stderr, "failed to setgid (%ld) setuid (%ld)\n", conn->cred.gid, conn->cred.uid);
		return -1;
	}

	/* allocate buffer */
	buf_size = russ_configparser_getint(config, section_name, "buffer_size", 0);
	buf_size = (buf_size < 32768) ? 32768: buf_size;
	if ((buf = malloc(sizeof(char)*buf_size)) == NULL) {
		goto free_vars;
	}
	if ((port = russ_configparser_getint(config, section_name, "port", -1)) < 0) {
		goto free_vars;
	}

	/* connect */
	if ((sd = connect_remote(hostname, port)) < 0) {
		goto free_vars;
	}

	/* send dial information */
	if (((n = enc_dial_info(conn, new_spath, buf, buf_size)) < 0)
		|| (fprintf(stderr, "enc_dial_info n (%d)\n", n) < 0)
		|| ((n = russ_writen(sd, buf, n)) < 0)) {
		goto free_vars;
	}

	forward_bytes(conn, sd);

	close(sd);
	return 0;

free_vars:
	free(buf);
	close(sd);
	return -1;
}

/**
* Service handler for /dial/<cluster>/<hostname>/* spaths.
*
* @param conn		connection object
*/
void
svc_dial_cluster_host_handler(struct russ_conn *conn) {
	struct russ_request	*req = NULL;
	char			*cluster_name = NULL, *hostname = NULL, *method = NULL;
	char			*new_spath = NULL, *p0 = NULL, *p1 = NULL, *p2 = NULL;
	char			section_name[256];
	int			exit_status;

	/* init */
	req = &(conn->req);
	p0 = req->spath+6;
	if (((p1 = strchr(p0, '/')) == NULL)
		|| ((p2 = strchr(p1+1, '/')) == NULL)
		|| ((cluster_name = strndup(p0, p1-p0)) == NULL)
		|| ((hostname = strndup(p1+1, p2-(p1+1))) == NULL)) {
		goto free_vars;
	}

	new_spath = p2;
	if ((snprintf(section_name, sizeof(section_name)-1, "cluster.%s", cluster_name) < 0)
		|| ((method = russ_configparser_get(config, section_name, "method", NULL)) == NULL)) {
		goto free_vars;
	}
	exit_status = __dial_remote(conn, new_spath, section_name, cluster_name, hostname);
	russ_conn_exit(conn, exit_status);

free_vars:
	free(method);
	free(cluster_name);
	free(hostname);
	russ_conn_exit(conn, RUSS_EXIT_FAILURE);
}

/**
* Master handler which selects service handler.
*
* @param conn		connection object
*/
void
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	int			rv;

	req = &(conn->req);
	if (strncmp(req->spath, "/dial/", 6) == 0) {
		/* service /dial/ for any op */
		svc_dial_cluster_host_handler(conn);
	} else if (strcmp(req->op, "execute") == 0) {
		if (strcmp(req->spath, "/debug") == 0) {
			svc_debug_handler(conn);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else if (strcmp(req->op, "help") == 0) {
		russ_dprintf(conn->fds[1], HELP);
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
	} else if (strcmp(req->op, "list") == 0) {
		if (strcmp(req->spath, "/") == 0) {
			russ_dprintf(conn->fds[1], "/debug\n/dial\n");
			russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		} else if (strcmp(req->spath, "/dial") == 0) {
			svc_dial_handler(conn);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else {
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
	}
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_srelay <config_filename>\n"
"\n"
"Relay client service. Connects local and remote hosts.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_listener	*lis;
	char			*filename, *path;
	int			mode, uid, gid;

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(-1);
	}

	filename = argv[1];
	if ((config = russ_configparser_read(filename)) == NULL) {
		fprintf(stderr, "error: could not read config file\n");
		exit(-1);
	}

	mode = russ_configparser_getsint(config, "server", "mode", 0600);
	uid = russ_configparser_getint(config, "server", "uid", getuid());
	gid = russ_configparser_getint(config, "server", "gid", getgid());
	path = russ_configparser_get(config, "server", "path", NULL);
	if ((lis = russ_announce(path, mode, uid, gid)) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_listener_loop(lis, master_handler);
	exit(0);
}
