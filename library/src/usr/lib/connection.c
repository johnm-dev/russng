/*
** lib/connection.c
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
	conn->creds.pid = -1;
	conn->creds.uid = -1;
	conn->creds.gid = -1;
	conn->sd = -1;
	russ_fds_init(conn->fds, RUSS_CONN_NFDS, -1);

	return conn;
free_conn:
	free(conn);
	return NULL;
}

/**
* Receive fds from connection.
*
* @param self		connection object
* @param deadline	deadline for operation
* @return		0 on success; -1 on error
*/
static int
russ_conn_recvfds(struct russ_conn *self, russ_deadline deadline) {
	char	buf[32+RUSS_CONN_NFDS], *bp, *bend;
	int	nfds, i;

	/* recv count of fds and fd statuses */
	if ((russ_readn_deadline(deadline, self->sd, buf, 4) < 4)
		|| (russ_dec_i(buf, &nfds) == NULL)
		|| (nfds > RUSS_CONN_NFDS)
		|| (russ_readn_deadline(deadline, self->sd, buf, nfds) < nfds)) {
		return -1;
	}

	/* initialize and recv fds and load first nfds */
	russ_fds_init(self->fds, RUSS_CONN_NFDS, -1);
	for (i = 0; i < nfds; i++) {
		if ((buf[i]) && (russ_recvfd(self->sd, &(self->fds[i])) < 0)) {
			return -1;
		}
	}
	return 0;
}

/**
* Send first nfds fds over connection and cleanup.
*
* Each cfd has an sfd counterpart. The cfds are sent over the
* connection and closed. The sfds are saved to the connection
* object. 
*
* @param self		connection object
* @param nfds		number of fds to send (from index 0)
* @param cfds		client-side descriptors
* @param sfds		server-side descriptors
* @return		0 on success; -1 on error
*/
int
russ_conn_sendfds(struct russ_conn *self, int nfds, int *cfds, int *sfds) {
	char	buf[32+RUSS_CONN_NFDS], *bp, *bend;
	int	i;

	/* find "real" nfds (where fd>=0) */
	for (; (nfds > 0) && (cfds[nfds-1] < 0); nfds--);
	if (nfds > RUSS_CONN_NFDS) {
		return -1;
	}

	/* encode nfds and statuses and send*/
	if ((bp = russ_enc_i(buf, buf+sizeof(buf), nfds)) == NULL) {
		return -1;
	}
	for (i = 0; i < nfds; i++) {
		bp[i] = (char)(cfds[i] >= 0);
	}
	bp += nfds;
	if (russ_writen_deadline(RUSS_DEADLINE_NEVER, self->sd, buf, bp-buf) < bp-buf) {
		return -1;
	}

	/* send fds */
	for (i = 0; i < nfds; i++) {
		if (cfds[i] < 0) {
			continue;
		}
		if (russ_sendfd(self->sd, cfds[i]) < 0) {
			return -1;
		}
		russ_fds_close(&cfds[i], 1);
		if (sfds) {
			self->fds[i] = sfds[i];
		}
	}
	return 0;
}

/**
* Answer request and close socket.
*
* @param self		accepted connection object
* @param nfds		number of elements in cfds (and sfds) array
* @param cfds		array of descriptors to send to client
* @param sfds		array of descriptors for server side
* @return		0 on success; -1 on error
*/
int
russ_conn_answer(struct russ_conn *self, int nfds, int *cfds, int *sfds) {
	if ((nfds < 0) || (russ_conn_sendfds(self, nfds, cfds, sfds) < 0)) {
		russ_fds_close(&self->sd, 1);
		return -1;
	}
	russ_fds_close(&self->sd, 1);
	return 0;
}

/**
* Pass fds from a server to a client.
*
* This function facilitates splicing a server connection and a
* client connection by passing fds from one to the other,
* respectively. It is typically used for alternate accept handlers
* by servers which connect client and server then get out of the
* way and exit (e.g., redirect).
*
* @param self		connection object
* @param dconn		dialed connection object
*/
int
russ_conn_splice(struct russ_conn *self, struct russ_conn *dconn) {
	int	cfds[RUSS_CONN_NFDS];
	int	i, ev;

	for (i = 0; i < RUSS_CONN_NFDS; i++) {
		cfds[i] = dconn->fds[i];
	}
	ev = russ_conn_sendfds(self, RUSS_CONN_NFDS, cfds, NULL);

	/* dconn fds are closed by russ_conn_sendfds */
	russ_fds_close(&dconn->sd, 1);

	/* close sd and fds */
	russ_fds_close(self->fds, RUSS_CONN_NFDS);
	russ_fds_close(&self->sd, 1);

	return ev;
}

/**
* Wait for the request.
*
* The request is waited for, and the connection object is updated
* with the received information.
*
* @param self		connection object
* @param deadline	deadline to wait
* @return		request object; NULL on failure
*/
struct russ_req *
russ_conn_await_request(struct russ_conn *self, russ_deadline deadline) {
	struct russ_req		*req;
	char			buf[RUSS_REQ_BUF_MAX], *bp;
	int			alen, size;
	char			*dummy;

	/* get request size, load, and upack */
	bp = buf;
	if ((russ_readn_deadline(deadline, self->sd, bp, 4) < 0)
		|| ((bp = russ_dec_i(bp, &size)) == NULL)
		|| (russ_readn_deadline(deadline, self->sd, bp, size) < 0)) {
		return NULL;
	}

	if ((req = russ_req_new(NULL, NULL, NULL, NULL, NULL)) == NULL) {
		/* TODO: what about the connection? */
		return NULL;
		goto free_request;
	}

	dummy = NULL;
	if (((bp = russ_dec_s(bp, &(req->protocol_string))) == NULL)
		|| (strcmp(RUSS_REQ_PROTOCOL_STRING, req->protocol_string) != 0)
		|| ((bp = russ_dec_b(bp, &dummy)) == NULL)
		|| ((bp = russ_dec_s(bp, &(req->spath))) == NULL)
		|| ((bp = russ_dec_s(bp, &(req->op))) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &(req->attrv), &alen)) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &(req->argv), &alen)) == NULL)) {

		goto free_request;
	}
	free(dummy);
	req->opnum = russ_opnum_lookup(req->op);
	return req;
free_request:
	free(dummy);
	russ_req_free(req);
	return NULL;
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
	russ_fds_close(self->fds, RUSS_CONN_NFDS);
	russ_fds_close(&self->sd, 1);
}

/**
* Send exit information to client.
*
* An exit status (integer) and exit string are sent over the exit fd
* to the client. The exit fd is then closed. This operation is valid
* for the server side only.
*
* Note: the other (non-exit fd) fds are not affected.
*
* @param self		connection object
* @param exit_status	exit status
* @return		0 on success; -1 on failure
*/
int
russ_conn_exit(struct russ_conn *self, int exit_status) {
	char	buf[1024];
	char	*exit_string = "";
	char	*bp, *bend;

	if (self->fds[3] < 0) {
		return -1;
	}
	bp = buf;
	bend = bp+sizeof(buf);
	if (((bp = russ_enc_i(bp, bend, exit_status)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, exit_string)) == NULL)) {
		// error?
		return -1;
	}
	if (russ_writen(self->fds[3], buf, bp-buf) < bp-buf) {
		return -1;
	}
	russ_fds_close(&self->fds[3], 1);
	return 0;
}

/**
* Helper routine to write error message and exit status.
*
* An error message is sent to the connection error fd (with a
* trailing newline) and the exit_status over the exit fd. If the
* exit fd is already closed, then no message is written or exit
* status sent.
*
* @param self		connection object
* @param msg		message string (no newline)
* @param exit_status	exit status
* @return		0 on success; -1 on failure
*/
int
russ_conn_exits(struct russ_conn *self, char *msg, int exit_status) {
	if (self->fds[3] < 0) {
		return -1;
	}
	russ_dprintf(self->fds[2], "%s\n", msg);
	return russ_conn_exit(self, exit_status);
}

/**
* Helper routine to write error message, write exit status, and
* close connection fds.
*
* Calls russ_conn_exits() followed by russ_conn_close().
*
* @param self		connection object
* @param msg		message string (no newline)
* @param exit_status	exit status
* @return		0 on success; -1 on failure (but the
*			connection object is closed)
*/
int
russ_conn_fatal(struct russ_conn *self, char *msg, int exit_status) {
	int	ev;

	ev = russ_conn_exits(self, msg, exit_status);
	russ_conn_close(self);
	return ev;
}

/**
* Wait for exit information.
*
* Wait on the exit fd for the exit status (integer) and exit string.
* This operation is valid for the client side only. The exit fd is
* closed once the information is received.
*
* Note: the other (non-exit fd) fds are not affected.
*
* @param self		connection object
* @param[out] exit_status
			exit status
* @param deadline	deadline to wait
* @return		0 on success; on -1 general failure; -2 on exit fd closed; -3 on deadline expired
*/
int
russ_conn_wait(struct russ_conn *self, int *exit_status, russ_deadline deadline) {
	struct pollfd	poll_fds[1];
	char		buf[1024];
	int		rv, _exit_status;

	if (self->fds[3] < 0) {
		return -2;
	}

	poll_fds[0].fd = self->fds[3];
	poll_fds[0].events = POLLIN;
	while (1) {
		rv = poll(poll_fds, 1, russ_to_deadline(deadline));
		if (rv == 0) {
			/* timeout */
			return -3;
		} else if (rv < 0) {
			if (errno != EINTR) {
				return -1;
			}
		} else {
			if (poll_fds[0].revents & POLLIN) {
				// TODO: should this be a byte or integer?
				if (russ_read(self->fds[3], buf, 4) < 0) {
					/* serious error; close fd? */
					return -1;
				}
				russ_dec_i(buf, &_exit_status);
				if (exit_status != NULL) {
					*exit_status = _exit_status;
				}
				/* TODO: exit_string is ignored */
				break;
			} else if (poll_fds[0].revents & POLLHUP) {
				return -2;
			}
		}
	}
	russ_fds_close(&self->fds[3], 1);
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
	free(self);
	return NULL;
}

/**
* Send request.
*
* Request information is encoded and sent over the connection.
*
* @param self		connection object
* @param deadline	deadline to send
* @param req		request object
* @return		0 on success; -1 on error
*/
int
russ_conn_send_request(struct russ_conn *self, russ_deadline deadline, struct russ_req *req) {
	char	buf[RUSS_REQ_BUF_MAX], *bp, *bend;

	bp = buf;
	bend = buf+sizeof(buf);
	if ((req == NULL)
		|| ((bp = russ_enc_i(bp, bend, 0)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, req->protocol_string)) == NULL)
		|| ((bp = russ_enc_b(bp, bend, NULL, 0)) == NULL) /* dummy */
		|| ((bp = russ_enc_s(bp, bend, req->spath)) == NULL)
		|| ((bp = russ_enc_s(bp, bend, req->op)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->attrv)) == NULL)
		|| ((bp = russ_enc_sarray0(bp, bend, req->argv)) == NULL)) {
		//|| ((bp = russ_enc_sarrayn(bp, bend, req->argv, req->argc)) == NULL)) {
		return -1;
	}

	/* patch size and send */
	russ_enc_i(buf, bend, bp-buf-4);
	if (russ_writen_deadline(deadline, self->sd, buf, bp-buf) < bp-buf) {
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
* @param deadline	deadline to complete operation
* @param op		operation string
* @param spath		service path
* @param attrv		NULL-terminated array of attributes ("name=value" strings)
* @param argv		NULL-terminated array of arguments
* @return		connection object; NULL on failure
*/
struct russ_conn *
russ_dialv(russ_deadline deadline, char *op, char *spath, char **attrv, char **argv) {
	struct russ_conn	*conn;
	struct russ_req		*req;
	struct russ_target	*targ;
	char			*saddr, *spath2;

	if (russ_spath_split(spath, &saddr, &spath2) < 0) {
		return NULL;
	}
	if (((conn = russ_conn_new()) == NULL)
		|| ((conn->sd = russ_connectunix_deadline(deadline, saddr)) < 0)) {
		goto free_saddr;
	}

	if (((req = russ_req_new(RUSS_REQ_PROTOCOL_STRING, op, spath2, attrv, argv)) == NULL)
		|| (russ_conn_send_request(conn, deadline, req) < 0)
		|| (russ_conn_recvfds(conn, deadline) < 0)) {
		goto free_request;
	}
	free(saddr);
	free(spath2);
	russ_fds_close(&conn->sd, 1);	/* sd not needed anymore */
	russ_req_free(req);
	return conn;

free_request:
	russ_req_free(req);
close_conn:
	russ_conn_close(conn);
	free(conn);
free_saddr:
	free(saddr);
	free(spath2);
	return NULL;
}

/**
* Dial service using variable argument list.
*
* See dialv() for more.
*
* @param deadline	deadline to complete operation
* @param op		operation string
* @param spath		service path
* @param attrv		array of attributes (as name=value strings)
* @param ...		variable argument list of "char *" with NULL sentinel
* @return		connection object, NULL on failure
*/
struct russ_conn *
russ_diall(russ_deadline deadline, char *op, char *spath, char **attrv, ...) {
	struct russ_conn	*conn;
	va_list			ap;
	void			*p;
	int			i, argc;
	char			**argv;

	/* count args */
	va_start(ap, attrv);
	for (argc = 0; argc < RUSS_REQ_ARGS_MAX; argc++) {
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

	conn = russ_dialv(deadline, op, spath, attrv, argv);
	free(argv);

	return conn;
}
