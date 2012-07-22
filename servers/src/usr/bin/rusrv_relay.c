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
	#define ssh_channel_is_closed(a) channel_is_closed((a))
	#define ssh_channel_send_eof(a) channel_send_eof((a))
	#define ssh_channel_read_nonblocking(a,b,c,d) channel_read_nonblocking((a),(b),(c),(d))
	#define ssh_channel_get_exit_status(a) channel_get_exit_status((a))
#endif

/* global */
struct configparser	*config;

char	*HELP = 
"Relays requests between hosts.\n"
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
* Forward bytes between connection and ssh chan.
*
* @param conn		connection object
* @param ssh_chan	established ssh_chan
* @param buf		working buffer
* @param buf_size	size of working buffer
* @return		0 on success; -1 on failure
*/
int
forward_bytes_over_ssh(struct russ_conn *conn, ssh_channel ssh_chan, char *buf, int buf_size) {
	ssh_channel	in_chans[2];
	ssh_channel	out_chans[2];
	fd_set		readfds;
	struct timeval	tv;
	int		maxfd, rv;
	int		nread, nwrite;
	int		ready_out, ready_err;
	int		exit_status = RUSS_EXIT_SYS_FAILURE;

	in_chans[0] = ssh_chan;
	in_chans[1] = NULL;
	maxfd = 0;
	ready_out = 1;
	ready_err = 1;

	while (1) {
		if (ssh_channel_is_closed(ssh_chan)) {
			/* from other size */
			break;
		}
#if 0
		if (ssh_channel_is_eof(ssh_chan)) {
			/* from other side */
			ssh_channel_close(ssh_chan);
			break;
		}
#endif
		if ((in_chans[0] == NULL) && (conn->fds[0] < 0)) {
			/* no more inputs, either way */
			ssh_channel_close(ssh_chan);
			break;
		}

		/* watch for inputs */
		do {
			maxfd = 0;
			FD_ZERO(&readfds);
			if (conn->fds[0] >= 0) {
				FD_SET(conn->fds[0], &readfds);
				maxfd = (conn->fds[0])+1;
			}

			tv.tv_sec = 5;
			tv.tv_usec = 0;
			rv = ssh_select(in_chans, out_chans, maxfd, &readfds, &tv);
fprintf(stderr, "rv (%d) in_chans[0] (%d) out_chans[0] (%d) conn->fds[0] (%d) ready_out (%d) ready_err (%d)\n", rv, in_chans[0], out_chans[0], conn->fds[0], ready_out, ready_err);
		} while (rv == SSH_EINTR);

		if (rv == SSH_ERROR) {
			ssh_channel_close(ssh_chan);
			break;
		}

		/* conn stdin */
		if (FD_ISSET(conn->fds[0], &readfds)) {
			nread = russ_read(conn->fds[0], buf, buf_size);
			if (((nread > 0) && (ssh_channel_write(ssh_chan, buf, nread) != nread))
				|| (nread <= 0)) {
				/* short write or error/eof read; no more stdin */
				close(conn->fds[0]);
				conn->fds[0] = -1;
				ssh_channel_send_eof(ssh_chan);
			}
		}

		/* service ssh_chan */
		if (out_chans[0] != NULL) {
			/* ssh_chan stdout */
			if (ready_out) {
				nread = ssh_channel_read_nonblocking(ssh_chan, buf, buf_size, 0);
				if (((nread > 0) && (russ_writen(conn->fds[1], buf, nread) != nread))
					|| (nread < 0)) {
					/* error read or short write; no more stdout */
					if (conn->fds[1] >= 0) {
						close(conn->fds[1]);
						conn->fds[1] = -1;
					}
					ready_out = 0;
				}
			}

			/* ssh_chan stderr */
			if (ready_err) {
				nread = ssh_channel_read_nonblocking(ssh_chan, buf, buf_size, 1);
				if (((nread > 0) && (russ_writen(conn->fds[2], buf, nread) != nread))
					|| (nread < 0)) {
					/* error read or short write; no more stderr */
					if (conn->fds[2] >= 0) {
						close(conn->fds[2]);
						conn->fds[2] = -1;
					}
					ready_err = 0;
				}
			}
			/*
			* if local stdout is closed/unavailable,
			* the we must close the channel or else
			* data will continue to be sent and buffered
			* (along the way, e.g., in a pipe). There is
			* no way to communicate that one of stdout
			* or stderr are closed and that the other
			* end should stop using it. This prevents
			* libssh from being used as a general
			* purpose i/o transfer service for an
			* arbitrary number of fds
			*/
			if (!ready_out) {
				in_chans[0] = NULL;
				ssh_channel_close(ssh_chan);
			}
		}
	}
//fprintf(stderr, "relay forward EXITING rv (%d) is_closed (%d) is_eof (%d) exit_status (%d) ready_out (%d) ready_err (%d)\n", rv, ssh_channel_is_closed(ssh_chan), ssh_channel_is_eof(ssh_chan), ssh_channel_get_exit_status(ssh_chan), ready_out, ready_err);
	return 0;
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
* @return		0 on success; -1 on failure
*/
int
_dial_for_ssh(struct russ_conn *conn, char *new_spath, char *section_name, char *cluster_name, char *hostname) {
	ssh_session	ssh_sess = NULL;
	ssh_channel	ssh_chan = NULL;
	int		ssh_state;
	char		*subsystem_path = NULL;
	char		*buf;
	int		buf_size, nbytes;
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

	/* allocate buffer */
	buf_size = configparser_getint(config, section_name, "buffer_size", 0);
	buf_size = (buf_size < 32768) ? 32768: buf_size;
	if ((buf = malloc(sizeof(char)*buf_size)) == NULL) {
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
	if (((nbytes = enc_dial_info(conn, new_spath, buf, buf_size)) < 0)
		|| ((nbytes = ssh_channel_write(ssh_chan, buf, nbytes)) < 0)) {
		goto free_vars;
	}

	forward_bytes_over_ssh(conn, ssh_chan, buf, buf_size);

free_vars:
	free(buf);
	exit_status = ssh_channel_get_exit_status(ssh_chan);
	ssh_channel_send_eof(ssh_chan);
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
* @return		0 on success; -1 on failure
*/
int
_dial_for_ssl_key(struct russ_conn *conn, char *new_spath, char *section_name, char *cluster_name, char *hostname) {
	return -1;
}

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
		if ((section_names = configparser_sections(config)) == NULL) {
			russ_conn_exit(conn, RUSS_EXIT_FAILURE);
			return;
		}
		for (p = section_names; *p != NULL; p++) {
			if (strncmp(*p, "cluster.", 8) == 0) {
				russ_dprintf(conn->fds[1], "%s\n", (*p)+8);
			}
		}
		configparser_sarray0_free(section_names);
	}
	russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
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
		|| ((method = configparser_get(config, section_name, "method", NULL)) == NULL)) {
		goto free_vars;
	}
	if (strcmp(method, "ssh") == 0) {
		exit_status = _dial_for_ssh(conn, new_spath, section_name, cluster_name, hostname);
	} else if (strcmp(method, "ssl-key") == 0) {
		exit_status = _dial_for_ssl_key(conn, new_spath, section_name, cluster_name, hostname);
	}
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
"usage: rusrv_relayc <config_filename>\n"
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
	if ((config = configparser_read(filename)) == NULL) {
		fprintf(stderr, "error: could not read config file\n");
		exit(-1);
	}

	mode = configparser_getsint(config, "server", "mode", 0600);
	uid = configparser_getint(config, "server", "uid", getuid());
	gid = configparser_getint(config, "server", "gid", getgid());
	path = configparser_get(config, "server", "path", NULL);
	if ((lis = russ_announce(path, mode, uid, gid)) == NULL) {
		fprintf(stderr, "error: cannot announce service\n");
		exit(-1);
	}
	russ_listener_loop(lis, master_handler);
}