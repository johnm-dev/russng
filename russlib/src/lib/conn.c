/*
** lib/conn.c
*/

/*
# license--start
#
# This file is part of the RUSS library.
# Copyright (C) 2012 John Marshall
#
# The RUSS library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end
*/

#include <errno.h>
#include <libgen.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Connect to socket.
*
* @param saddr		"unresolved" socket address
* @return		descriptor value; -1 on error
*/
static int
__connect(char *saddr) {
	struct sockaddr_un	servaddr;
	int			sd;

	/* returned path must be freed */
	if ((saddr = russ_resolve_addr(saddr)) == NULL) {
		return -1;
	}
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0) {
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sun_family = AF_UNIX;
		strcpy(servaddr.sun_path, saddr);
		if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
			close(sd);
			sd = -1;
		}
	}
	free(saddr);
	return sd;
}

/**
* Close a descriptor of the connection.
*
* @param self		connection object
* @param index		index of the descriptor
*/
void
russ_conn_close_fd(struct russ_conn *self, int index) {
	russ_fds_close(&(self->fds[index]), 1);
}

/**
* Create and initialize a connection object.
*
* @return		a new, initialized connection object; NULL on failure
*/
struct russ_conn *
russ_conn_new(void) {
	struct russ_conn	*conn;

	if ((conn = malloc(sizeof(struct russ_conn))) == NULL) {
		return NULL;
	}
	conn->cred.pid = -1;
	conn->cred.uid = -1;
	conn->cred.gid = -1;
	if (russ_request_init(&(conn->req), NULL, NULL, NULL, NULL, NULL) < 0) {
		goto free_request;
	}
	conn->sd = -1;
	conn->exit_fd = -1;
	conn->nfds = RUSS_CONN_NFDS;
	russ_fds_init(conn->fds, conn->nfds, -1);

	return conn;
free_request:
	russ_request_free_members(&(conn->req));
free_conn:
	free(conn);
	return NULL;
}

/**
* Get fds from connection.
*
* @param self		connection object
* @return		0 on success; -1 on error
*/
static int
russ_conn_recvfds(struct russ_conn *self) {
	char	buf[32], *bp, *bend;
	int	timeout = -1; /* TODO: change API to accept timeout */
	int	nfds;
	int	i;

	/* recv count of fds */
	if ((russ_readn(self->sd, buf, 4) < 4)
		|| (russ_dec_i(buf, &nfds) == NULL)) {
		return -1;
	}

	/* recv fds (exit and cfds) */
	if (russ_recvfd(self->sd, &(self->exit_fd)) < 0) {
		return -1;
	}
	self->nfds = nfds-1;
	for (i = 0; i < self->nfds; i++) {
		if (russ_recvfd(self->sd, &(self->fds[i])) < 0) {
			return -1;
		}
	}
	return 0;
}

/**
* Send fds over connection and cleanup.
*
* Each cfd has an sfd counterpart. The cfds are sent over the
* connection and closed. The sfds are saved to the connection
* object.
*
* @param self		connection object
* @param cfds		client-side descriptors
* @param sfds		server-side descriptors
* @return		0 on success; -1 on error
*/
static int
russ_conn_sendfds(struct russ_conn *self, int nfds, int *cfds, int *sfds) {
	char	buf[32], *bp, *bend;
	int	timeout = -1; /* TODO: change API to accept timeout */
	int	i;

	/* send count of fds */
	if (((bp = russ_enc_i(buf, buf+sizeof(buf), nfds)) == NULL)
		|| (russ_writen_timeout(self->sd, buf, bp-buf, timeout) < bp-buf)) {
		return -1;
	}

	/* send fds (exit and cfds) */
	if (russ_sendfd(self->sd, cfds[0]) < 0) {
		return -1;
	}
	self->exit_fd = sfds[0];
	russ_fds_close(&cfds[0], 1);

	for (i = 1; i < nfds; i++) {
		if (russ_sendfd(self->sd, cfds[i]) < 0) {
			return -1;
		}
		russ_fds_close(&cfds[i], 1);
		self->fds[i-1] = sfds[i];
	}
	self->nfds = nfds-1;
	return 0;
}

/**
* Accept request and close socket
*
* @param self		answered connection object
* @param cfds		array of descriptors to send to client
* @param sfds		array of descriptors for server side
* @return		0 on success; -1 on error
*/
int
russ_conn_accept(struct russ_conn *self, int *cfds, int *sfds) {
	int	_cfds[RUSS_CONN_NFDS+1], _sfds[RUSS_CONN_NFDS+1];
	int	fds[2], tmpfd;
	int	i;

	if ((cfds == NULL) && (sfds == NULL)) {
		cfds = _cfds;
		sfds = _sfds;
		if (russ_make_pipes(RUSS_CONN_NFDS+1, cfds, sfds) < 0) {
			fprintf(stderr, "error: cannot create pipes\n");
			return -1;
		}
		/* swap fds for stdin */
		tmpfd = cfds[1];
		cfds[1] = sfds[1];
		sfds[1] = tmpfd;
	}

	if (russ_conn_sendfds(self, RUSS_CONN_NFDS+1, cfds, sfds) < 0) {
		goto close_fds;
	}
	fsync(self->sd);
	russ_fds_close(&self->sd, 1);
	return 0;

close_fds:
	russ_fds_close(cfds, RUSS_CONN_NFDS+1);
	russ_fds_close(sfds, RUSS_CONN_NFDS+1);
	russ_fds_close(&self->sd, 1);
	return -1;
}

/**
* Wait for the request.
*
* The request is waited for, and the connection object is updated
* with the received information.
*
* @param self		connection object
* @return		0 on success; -1 on error
*/
int
russ_conn_await_request(struct russ_conn *self) {
	struct russ_request	*req;
	char			buf[MAX_REQUEST_BUF_SIZE], *bp;
	int			alen, size;

	/* get request size, load, and upack */
	bp = buf;
	if ((russ_readn(self->sd, bp, 4) < 0)
		|| ((bp = russ_dec_i(bp, &size)) == NULL)
		|| (russ_readn(self->sd, bp, size) < 0)) {
		return -1;
	}

	req = &(self->req);
	if (((bp = russ_dec_s(bp, &(req->protocol_string))) == NULL)
		|| (strcmp(RUSS_PROTOCOL_STRING, req->protocol_string) != 0)
		|| ((bp = russ_dec_s(bp, &(req->spath))) == NULL)
		|| ((bp = russ_dec_s(bp, &(req->op))) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &(req->attrv), &alen)) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &(req->argv), &alen)) == NULL)) {

		goto free_request;
	}
	return 0;
free_request:
	russ_request_free_members(&(self->req));
	return -1;
}

/**
* Close connection.
*
* All connection fds are closed and the connection object updated.
*
* @param self		connection object
*/
void
russ_conn_close(struct russ_conn *self) {
	russ_fds_close(self->fds, self->nfds);
	russ_fds_close(&self->exit_fd, 1);
	russ_fds_close(&self->sd, 1);
}

/**
* Send exit information to client.
*
* An exit status (integer) and exit string are sent over exit_fd to
* the client. The exit_fd is then closed. This operation is valid
* for the server side only.
*
* Note: the other (non-exit_fd) fds are not affected.
*
* @param self		connection object
* @param exit_status	exit status
* @param exit_string	optional exit string
* @return		0 on success; -1 on failure
*/
int
russ_conn_exit(struct russ_conn *self, int exit_status, char *exit_string) {
	char	buf[1024];
	char	*bp, *bend;

	if (self->exit_fd < 0) {
		return -1;
	}
	if (exit_string == NULL) {
		exit_string = "";
	}
	bp = buf;
	bend = bp+sizeof(buf);
	if (((bp = russ_enc_I(bp, bend, exit_status)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, exit_string)) == NULL)) {
		// error?
		return -1;
	}
	if (russ_write(self->exit_fd, buf, bp-buf) < bp-buf) {
		return -1;
	}
	russ_fds_close(&self->exit_fd, 1);
	return 0;
}

/**
* Wait for exit information.
*
* Wait on the exit_fd for the exit status (integer) and exit string.
* This operation is valid for the client side only. The exit_fd is
* closed once the information is received.
*
* Note: the other (non-exit_fd) fds are not affected.
*
* @param self		connection object
* @param exit_status[out]
*			exit status
* @param exit_string[out]
*			exit string
* @param timeout	timeout/deadline to wait
* @return		integer exit_status; -1 if exit_fd is closed
*/
int
russ_conn_wait(struct russ_conn *self, int *exit_status, char **exit_string, russ_timeout timeout) {
	struct pollfd	poll_fds[1];
	char		buf[1024];
	int		poll_timeout;
	int		rv, _exit_status;

	if (self->exit_fd < 0) {
		return -1;
	}
	poll_fds[0].fd = self->exit_fd;
	poll_fds[0].events = POLLIN;
	poll_timeout = (int)timeout;
	while (1) {
		rv = poll(poll_fds, 1, poll_timeout);
		if (rv == 0) {
			/* timeout */
			return -1;
		} else if (rv < 0) {
			if (errno != EINTR) {
				return -1;
			}
		} else {
			if (poll_fds[0].revents && POLLIN) {
				// TODO: should this be a byte or integer?
				if (russ_read(self->exit_fd, buf, 4) < 0) {
					/* serious error; close fd? */
					return -1;
				}
				russ_dec_I(buf, &_exit_status);
				if (exit_status != NULL) {
					*exit_status = _exit_status;
				}
				if (exit_string != NULL) {
					*exit_string = NULL;
				}
				break;
			}
		}
	}
	russ_fds_close(&self->exit_fd, 1);
	return 0;
}

/**
* Free connection object.
*
* @param self		connection object
* @return		NULL value
*/
struct russ_conn *
russ_conn_free(struct russ_conn *self) {
	russ_request_free_members(&(self->req));
	free(self);
	return NULL;
}

/**
* Send request.
*
* Request information is encoded and sent over the connection.
*
* @param self		connection object
* @param timeout	time in which to complete the send
* @return		0 on success; -1 on error
*/
int
russ_conn_send_request(struct russ_conn *self, russ_timeout timeout) {
	struct russ_request	*req;
	char			buf[MAX_REQUEST_BUF_SIZE], *bp, *bend;

	req = &(self->req);
	bp = buf;
	bend = buf+sizeof(buf);
	if (((bp = russ_enc_i(bp, bend, 0)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, req->protocol_string)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, req->spath)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, req->op)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->attrv)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->argv)) == NULL)) {
		//|| ((bp = russ_enc_sarrayn(bp, bend, req->argv, req->argc)) == NULL)) {
		return -1;
	}

	/* patch size and send */
	russ_enc_i(buf, bend, bp-buf-4);
	if (russ_writen_timeout(self->sd, buf, bp-buf, timeout) < bp-buf) {
		return -1;
	}
	return 0;
}

/**
* Dial service.
*
* Connect to a service, send request information, and get fds.
* Received fds are saved to the connection object.
*
* @param timeout	time allowed to complete operation
* @param op		operation string
* @param addr		full service address
* @param attrv		NULL-terminated array of attributes ("name=value" strings)
* @param argv		NULL-terminated array of arguments
* @return		connection object; NULL on failure
*/
struct russ_conn *
russ_dialv(russ_timeout timeout, char *op, char *addr, char **attrv, char **argv) {
	struct russ_conn	*conn;
	struct russ_request	*req;
	struct russ_target	*targ;
	char			*path, *spath;

	if ((targ = russ_find_service_target(addr)) == NULL) {
		return NULL;
	}

	/* steps to set up conn object */
	if ((conn = russ_conn_new()) == NULL) {
		goto free_targ;
	}
	if (((conn->sd = __connect(targ->saddr)) < 0)
		|| (russ_request_init(&(conn->req), RUSS_PROTOCOL_STRING, op, targ->spath, attrv, argv) < 0)
		|| (russ_conn_send_request(conn, timeout) < 0)
		|| (russ_conn_recvfds(conn) < 0)) {
		goto free_request;
	}
	free(targ);
	russ_fds_close(&conn->sd, 1);	/* sd not needed anymore */
	return conn;

free_request:
	russ_request_free_members(&(conn->req));
close_conn:
	russ_conn_close(conn);
	free(conn);
free_targ:
	free(targ);
	return NULL;
}

/**
* Dial service using variable argument list.
*
* See dialv() for more.
*
* @param timeout	time allowed to complete operation
* @param op		operation string
* @param addr		full service address
* @param attrv		array of attributes (as name=value strings)
* @param ...		variable argument list of "char *" with NULL sentinel
* @return		connection object, NULL on failure
*/
struct russ_conn *
russ_diall(russ_timeout timeout, char *op, char *addr, char **attrv, ...) {
	struct russ_conn	*conn;
	va_list			ap;
	void			*p;
	int			i, argc;
	char			**argv;

	/* count args */
	va_start(ap, attrv);
	for (argc = 0; argc < RUSS_MAX_ARGC; argc++) {
		if ((p = va_arg(ap, char *)) == NULL) {
			break;
		}
	}
	va_end(ap);

	/* create argv */
	if ((argv = malloc(sizeof(char *)*argc)) == NULL) {
		return NULL;
	}
	va_start(ap, attrv);
	for (i = 0; i < argc; i++) {
		argv[i] = va_arg(ap, char *);
	}
	va_end(ap);

	conn = russ_dialv(timeout, addr, op, attrv, argv);
	free(argv);

	return conn;
}
