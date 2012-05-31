/*
** bin/rusrv_relay.c
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

#include <libssh/libssh.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "configparser.h"
#include "russ_priv.h"

/* use different function names for v0.4 */
#if LIBSSH_VERSION_MAJOR == 0 && LIBSSH_VERSION_MINOR == 4
	#define ssh_channel_new(a) channel_new((a))
	#define ssh_channel_open_session(a) channel_open_session((a))
	#define ssh_channel_request_exec(a,b) channel_request_exec((a),(b))
	#define ssh_channel_write(a,b,c) channel_write((a),(b),(c))
	#define ssh_channel_close(a) channel_close((a))
	#define ssh_channel_free(a) channel_free((a))
	#define ssh_channel_is_eof(a) channel_is_eof((a))
	#define ssh_channel_is_open(a) channel_is_open((a))
	#define ssh_channel_send_eof(a) channel_send_eof((a))
	#define ssh_channel_read_nonblocking(a,b,c,d) channel_read_nonblocking((a),(b),(c),(d))
#endif

/* global */
struct configparser	*config;

/**
* Forward bytes between connection and ssh chan.
*
* @param conn
*/
int
forward_bytes_over_ssh(struct russ_conn *conn, ssh_channel ssh_chan) {
	ssh_channel	in_chans[2];
	ssh_channel	out_chans[2];
	fd_set		readfds;
	int		maxfds;
	int		rv;
	struct timeval	tv;
	char		buf[1024];
	int		nbytes;

	tv.tv_sec = 60;
	tv.tv_usec = 0;

	in_chans[0] = ssh_chan;
	in_chans[1] = NULL;
	maxfds = (conn->fds[0])+1;

	while (!ssh_channel_is_eof(ssh_chan)) {
		do {
			/* register conn stdin fd */
			maxfds = 0;
			if (conn->fds[0] >= 0) {
				FD_ZERO(&readfds);
				FD_SET(conn->fds[0], &readfds);
				maxfds = (conn->fds[0])+1;
			}
			rv = ssh_select(in_chans, out_chans, maxfds, &readfds, &tv);
		} while (rv == SSH_EINTR);

		if (rv == SSH_ERROR) {
			break;
		}

#if 0
		printf("SSH_OK (%d) SSH_ERROR (%d) SSH_EINTR (%d)\n", SSH_OK, SSH_ERROR, SSH_EINTR);
		printf("rv (%d)\n", rv);
		printf("maxfds (%d) fds[0] (%d)\n", maxfds, conn->fds[0]);
		printf("out_chans (%p)\n", out_chans[0]);
		printf("FD_ISSET (%d)\n", FD_ISSET(conn->fds[0], &readfds));
		printf("ssh_chan (%p)\n", ssh_chan);
		printf("is_open (%d) is_eof (%d)\n", ssh_channel_is_open(ssh_chan), ssh_channel_is_eof(ssh_chan));
#endif

		/* conn stdin */
		if (FD_ISSET(conn->fds[0], &readfds)) {
			nbytes = read(conn->fds[0], buf, 8);
			if (nbytes) {
				ssh_channel_write(ssh_chan, buf, nbytes);
			} else {
				/* no more xfer from stdin */
				close(conn->fds[0]);
				conn->fds[0] = -1;
				ssh_channel_send_eof(ssh_chan);
			}
		}

		/* service ssh_chan */
		if (out_chans[0]) {
			/* ssh_chan stdout */
			nbytes = ssh_channel_read_nonblocking(ssh_chan, buf, sizeof(buf), 0);
			if (nbytes > 0) {
				write(conn->fds[1], buf, nbytes);
			} else if (nbytes < 0) {
				break;
			}

			/* ssh_chan stderr */
			nbytes = ssh_channel_read_nonblocking(ssh_chan, buf, sizeof(buf), 1);
			if (nbytes > 0) {
				write(conn->fds[2], buf, nbytes);
			} else if (nbytes < 0) {
				break;
			}
		}
	}
	ssh_channel_close(ssh_chan);
}

/**
* Encode dial information into buffer.
*
* @param conn		connection object
* @param buf		buffer
* @param buf_size	size of buffer
* @return		# of bytes encoded; -1 for failure
*/
int
enc_dial_info(struct russ_conn *conn, char *new_spath, char *buf, int buf_size) {
	struct russ_request	*req;
	char			*bp, *bend;

	req = &(conn->req);
	bend = buf+buf_size;
	bp = buf+4;
	if (((bp = russ_enc_s(bp, bend, req->op)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, new_spath)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->attrv)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->argv)) == NULL)) {
		return -1;
	}
	/* patch size */
	russ_enc_I(buf, bend, bp-buf-4);
	return bp-buf;
}

/**
* Dial relays using ssh method.
*
* @param conn		connection object
* @param section_name	config section name
* @param cluster_name	cluster name
* @param hostname	hostname
* @param spath		new spath for conn
* @return		0 for success; -1 for failure
*/
int
_dial_for_ssh(struct russ_conn *conn, char *new_spath, char *section_name, char *cluster_name, char *hostname) {
	ssh_session	ssh_sess = NULL;
	ssh_channel	ssh_chan = NULL;
	int		ssh_state;
	int		nbytes;
	char		*subsystem_path = NULL;
	char		buf[32768];
	int		exit_status = -1;

	/* prep */
	if ((conn->cred.gid == 0)
		|| (conn->cred.uid == 0)
		|| (setgid(conn->cred.gid) < 0)
		|| (setuid(conn->cred.uid) < 0)) {
		fprintf(stderr, "failed to setgid (%ld) setuid (%ld)\n", conn->cred.gid, conn->cred.uid);
		return -1;
	}

	if ((subsystem_path = configparser_get(config, section_name, "subsystem_path", NULL)) == NULL) {
		goto free_vars;
	}

	/* setup and connect */
	if ((ssh_sess = ssh_new()) == NULL) {
		goto free_vars;
	}
	ssh_options_set(ssh_sess, SSH_OPTIONS_HOST, hostname);
	if (ssh_connect(ssh_sess) != SSH_OK) {
		fprintf(stderr, "ssh error (%s)\n", ssh_get_error(ssh_sess));
		goto free_vars;
	}
	ssh_state = ssh_is_server_known(ssh_sess);
	switch (ssh_state) {
	case SSH_SERVER_KNOWN_OK:
		break;
	case SSH_SERVER_ERROR:
		fprintf(stderr, "ssh error (%s)\n", ssh_get_error(ssh_sess));
		goto free_vars;
	default:
		fprintf(stderr, "ssh_state != SSH_SERVER_KNOWN_OK\n");
	}

	/* authenticate */
	//if (ssh_userauth_pubkey(ssh_sess, NULL, NULL, NULL) != SSH_OK) {
	if (ssh_userauth_autopubkey(ssh_sess, NULL) != SSH_OK) {
		fprintf(stderr, "userauth_publickey failed\n");
		goto free_vars;
	}

	/* execute */
	if (((ssh_chan = ssh_channel_new(ssh_sess)) == NULL)
		|| (ssh_channel_open_session(ssh_chan) != SSH_OK)
		|| (ssh_channel_request_exec(ssh_chan, subsystem_path) != SSH_OK)) {
		goto free_vars;
	}

	/* send dial information */
	if (((nbytes = enc_dial_info(conn, new_spath, buf, sizeof(buf))) < 0)
		|| ((nbytes = ssh_channel_write(ssh_chan, buf, nbytes)) < 0)) {
		goto free_vars;
	}

	exit_status = forward_bytes_over_ssh(conn, ssh_chan);

free_vars:
	ssh_channel_close(ssh_chan);
	ssh_channel_free(ssh_chan);
	ssh_disconnect(ssh_sess);
	ssh_free(ssh_sess);

	return exit_status;
}

/**
* Dial relays service using ssl-key method.
*
* @param conn		connection object
* @param section_name	config section name
* @param cluster_name	cluster name
* @param hostname	hostname
* @param spath		new spath for conn
* @return		0 for success; -1 for failure
*/
int
_dial_for_ssl_key(struct russ_conn *conn, char *new_spath, char *section_name, char *cluster_name, char *hostname) {
	return -1;
}

/**
* Service handler for /debug .
*
* @param conn		connection object
* @return		0 for success; -1 for failure
*/
int
svc_debug_handler(struct russ_conn *conn) {
	russ_dprintf(conn->fds[1], "n/a\n");
	return 0;
}

/**
* Service handler for all non serviceable spaths.
*
* @param conn		connection object
* @return		0 for success; -1 for failure
*/
int
svc_error_handler(struct russ_conn *conn, char *msg) {
	russ_dprintf(conn->fds[2], msg);
	return 0;
}

/**
* Service handler for /dial .
*
* @param conn		connection object
* @return		0 for success; -1 for failure
*/
int
svc_dial_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	char			**section_names, **p;

	req = &(conn->req);
	if (strcmp(req->op, "list") == 0) {
		if ((section_names = configparser_sections(config)) == NULL) {
			exit(-1);
		}
		for (p = section_names; *p != NULL; p++) {
			if (strncmp(*p, "cluster.", 8) == 0) {
				russ_dprintf(conn->fds[1], "%s\n", (*p)+8);
			}
		}
		configparser_sarray0_free(section_names);
	}
	return 0;
}

/**
* Service handler for /dial/<cluster>/<hostname>/* spaths.
*
* @param conn		connection object
* @return		0 for success; -1 for failure
*/
int
svc_dial_cluster_host_handler(struct russ_conn *conn) {
	struct russ_request	*req = NULL;
	char			*cluster_name = NULL, *hostname = NULL, *method = NULL;
	char			*new_spath = NULL, *p0 = NULL, *p1 = NULL, *p2 = NULL;
	char			section_name[256];
	int			exit_status = -1;

	/* init */
	req = &(conn->req);
	p0 = req->spath+6;
	if (((p1 = strchr(p0, '/')) == NULL)
		|| ((p2 = strchr(p1+1, '/')) == NULL)
		|| ((cluster_name = strndup(p0, p1-p0)) == NULL)
		|| ((hostname = strndup(p1+1, p2-(p1+1))) == NULL)) {
//printf("p0 (%p) (%s) p1 (%p) (%s) p2 (%p) (%s)\n", p0, p0, p1, p1, p2, p2);
		goto free_vars;
	}

//printf("p0 (%s) cluster_name (%s) hostname (%s) p2 (%s)\n", p0, cluster_name, hostname, p2);
	new_spath = p2;
	if ((snprintf(section_name, sizeof(section_name)-1, "cluster.%s", cluster_name) < 0)
		|| ((method = configparser_get(config, section_name, "method", NULL)) == NULL)) {
		goto free_vars;
	}
	if (strcmp(method, "ssh") == 0) {
		exit_status = _dial_for_ssh(conn, new_spath, section_name, cluster_name, hostname);
	} else if (strcmp(method, "ssl-key") == 0) {
		exit_status = _dial_for_ssl_key(conn, new_spath, section_name, cluster_name, hostname);
	}

free_vars:
	free(method);
	free(cluster_name);
	free(hostname);
	return exit_status;
}

/**
* Master handler which selects service handler.
*
* @param conn		connection object
* @return		0 for success; -1 for failure
*/
int
master_handler(struct russ_conn *conn) {
	struct russ_request	*req;
	int			rv;

	req = &(conn->req);
	if (strncmp(req->spath, "/dial/", 6) == 0) {
		/* service /dial/ for any op */
		rv = svc_dial_cluster_host_handler(conn);
	} else if (strcmp(req->op, "execute") == 0) {
		if (strcmp(req->spath, "/debug") == 0) {
			rv = svc_debug_handler(conn);
		} else {
			rv = svc_error_handler(conn, "error: unknown service\n");
		}
	} else if (strcmp(req->op, "help") == 0) {
		russ_dprintf(conn->fds[1], "see server usage for details\n");
		rv = 0;
	} else if (strcmp(req->op, "list") == 0) {
		if (strcmp(req->spath, "") == 0) {
			russ_dprintf(conn->fds[1], "/debug\n/dial\n");
		} else if (strcmp(req->spath, "/dial") == 0) {
printf("op (%s) spath (%s)\n", req->op, req->spath);
			rv = svc_dial_handler(conn);
		}
		rv = 0;
	} else {
		rv = svc_error_handler(conn, "error: unsupported operation\n");
	}
	return rv;
}

void
print_usage(char **argv) {
	fprintf(stderr,
"usage: rusrv_relayc <config_filename>\n"
"\n"
"Relay client service. Connects local and remote hosts.\n"
);
}

#if 0
"\n"
"/debug         output debugging information to stdout\n"
"/dial/<cluster>/<hostname>\n"
"               dial host as part of cluster\n"
#endif

#if 0
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
	russ_listener_loop(lis, master_handler);
}
#endif

int
main(int argc, char **argv) {
	struct russ_listener	*lis;
	char			*filename, *path;
	int			mode, uid, gid;

	signal(SIGCHLD, SIG_IGN);

	if (argc != 2) {
		print_usage(argv);
		exit(-1);
	}

	filename = argv[1];
	if ((config = configparser_read(filename)) == NULL) {
		fprintf(stderr, "error: could not read config file\n");
		exit(-1);
	}

	mode = configparser_getsint(config, "server", "mode", 0600);
printf("mode (%d)\n", mode);
	uid = configparser_getint(config, "server", "uid", getuid());
	gid = configparser_getint(config, "server", "gid", getgid());
	path = configparser_get(config, "server", "path", NULL);
	if ((lis = russ_announce(path, mode, uid, gid)) == NULL) {
		fprintf(stderr, "erro: cannot announce service\n");
		exit(-1);
	}
	russ_listener_loop(lis, master_handler);
}
