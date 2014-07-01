/*
* lib/sconn.c
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
* Free server connection object.
*
* @param self		server connection object
* @return		NULL value
*/
struct russ_sconn *
russ_sconn_free(struct russ_sconn *self) {
	self = russ_free(self);
	return NULL;
}

/**
* Create and initialize a server connection object.
*
* @return		a new, initialized server connection object;
*			NULL on failure
*/
struct russ_sconn *
russ_sconn_new(void) {
	struct russ_sconn	*sconn;

	if ((sconn = malloc(sizeof(struct russ_sconn))) == NULL) {
		return NULL;
	}
	sconn->creds.pid = -1;
	sconn->creds.uid = -1;
	sconn->creds.gid = -1;
	sconn->sd = -1;
	russ_fds_init(sconn->fds, RUSS_CONN_NFDS, -1);

	return sconn;
}

/**
* Close a descriptor of the connection.
*
* @param self		server connection object
* @param index		index of the descriptor
*/
void
russ_sconn_close_fd(struct russ_sconn *self, int index) {
	russ_fds_close(&(self->fds[index]), 1);
}

/**
* Send first nfds fds over the server connection and cleanup.
*
* Each cfd has an sfd counterpart. The cfds are sent over the
* connection and closed. The sfds are saved to the connection
* object.
*
* Note: This is used for sending sysfds and fds.
*
* @param self		server connection object
* @param nfds		number of fds to send (from index 0)
* @param cfds		client-side descriptors
* @return		0 on success; -1 on error
*/
int
russ_sconn_sendfds(struct russ_sconn *self, int nfds, int *cfds) {
	char	buf[32+RUSS_CONN_MAX_NFDS], *bp, *bend;
	int	i;

	/* find "real" nfds (where fd>=0) */
	for (; (nfds > 0) && (cfds[nfds-1] < 0); nfds--);
	if (nfds > RUSS_CONN_MAX_NFDS) {
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
	}
	return 0;
}

/**
* Answer request and close socket.
*
* System fds are created and sent; supplied I/O fds are sent.
*
* @param self		accepted server connection object
* @param nfds		number of elements in cfds (and sfds) array
* @param cfds		array of descriptors to send to client
* @param sfds		array of descriptors for server side
* @return		0 on success; -1 on error
*/
int
russ_sconn_answer(struct russ_sconn *self, int nfds, int *cfds, int *sfds) {
	int	csysfds[RUSS_CONN_NSYSFDS], ssysfds[RUSS_CONN_NSYSFDS];
	int	i;

	if (nfds < 0) {
		return -1;
	}

	/* set up system fds */
	russ_fds_init(csysfds, RUSS_CONN_NSYSFDS, -1);
	russ_fds_init(ssysfds, RUSS_CONN_NSYSFDS, -1);
	if (russ_make_pipes(RUSS_CONN_NSYSFDS, csysfds, ssysfds) < 0) {
		fprintf(stderr, "error: cannot create pipes\n");
		return -1;
	}
	/* copy server-side sysfds */
	self->sysfds[RUSS_CONN_SYSFD_EXIT] = ssysfds[RUSS_CONN_SYSFD_EXIT];

	if ((russ_sconn_sendfds(self, RUSS_CONN_NSYSFDS, csysfds) < 0)
		|| (russ_sconn_sendfds(self, nfds, cfds) < 0)) {
		russ_fds_close(csysfds, RUSS_CONN_NSYSFDS);
		russ_fds_close(ssysfds, RUSS_CONN_NSYSFDS);
		russ_fds_close(&self->sd, 1);
		return -1;
	}
	/* copy server-side fds */
	for (i = 0; i < nfds; i++) {
		self->fds[i] = sfds[i];
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
* @param self		server connection object
* @param dconn		dialed client connection object
*/
int
russ_sconn_splice(struct russ_sconn *self, struct russ_cconn *dconn) {
	int	ev = 0;

	/* send sysfds and fds */
	if ((russ_sconn_sendfds(self, RUSS_CONN_NSYSFDS, dconn->sysfds) < 0)
		|| (russ_sconn_sendfds(self, RUSS_CONN_NFDS, dconn->fds) < 0)) {
		ev = -1;
	}
	/* close fds */
	russ_fds_close(self->sysfds, RUSS_CONN_NSYSFDS);
	russ_fds_close(self->fds, RUSS_CONN_NFDS);

	/* close sockets */
	russ_fds_close(&dconn->sd, 1);
	russ_fds_close(&self->sd, 1);

	return ev;
}

/**
* Wait for the request.
*
* The request is waited for, and the connection object is updated
* with the received information.
*
* @param self		server connection object
* @param deadline	deadline to wait
* @return		request object; NULL on failure
*/
struct russ_req *
russ_sconn_await_request(struct russ_sconn *self, russ_deadline deadline) {
	struct russ_req		*req;
	char			buf[RUSS_REQ_BUF_MAX], *bp = NULL;
	int			size;

	/* need to get request size to load buffer */
	if ((russ_readn_deadline(deadline, self->sd, buf, 4) < 0)
		|| ((bp = russ_dec_i(buf, &size)) == NULL)
		|| (russ_readn_deadline(deadline, self->sd, bp, size) < 0)
		|| ((bp = russ_dec_req(buf, &req)) == NULL)) {
		/* TODO: what about the connection? */
		return NULL;
	}
	return req;
}

/**
* Close server connection.
*
* All connection fds are closed and the server connection object
* updated.
*
* @param self		server connection object
*/
void
russ_sconn_close(struct russ_sconn *self) {
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
* @param self		server connection object
* @param exit_status	exit status
* @return		0 on success; -1 on failure
*/
int
russ_sconn_exit(struct russ_sconn *self, int exit_status) {
	char	buf[32], *bp;

	bp = buf;
	if ((self->sysfds[RUSS_CONN_SYSFD_EXIT] < 0)
		|| ((bp = russ_enc_exit(bp, bp+sizeof(buf), exit_status)) == NULL)
		|| (russ_writen(self->sysfds[RUSS_CONN_SYSFD_EXIT], buf, bp-buf) < bp-buf)) {
		return -1;
	}
	russ_fds_close(&self->sysfds[RUSS_CONN_SYSFD_EXIT], 1);
	return 0;
}

/**
* Helper routine to write final message, write exit status, and
* close connection fds.
*
* @param self		server connection object
* @param msg		message string to connection stderr (newline
*			will be added)
* @param exit_status	exit status
* @return		0 on success; -1 on failure (but the server
*			connection object is closed)
*/
int
russ_sconn_fatal(struct russ_sconn *self, const char *msg, int exit_status) {
	russ_dprintf(self->fds[2], "%s\n", msg);
	return russ_sconn_exit(self, exit_status);
}

/**
* Dial a service and splice its client connection fds into the given
* server connection fds. The real and effective uid/gid are also
* set.
*
* @param self		server connection object
* @param req		request object
* @return		0 on success; -1 on failure
*/
int
russ_sconn_redial_and_splice(struct russ_sconn *self, russ_deadline deadline, struct russ_req *req) {
	struct russ_cconn	*cconn;

	/* switch user */
	if (russ_switch_user(self->creds.uid, self->creds.gid, 0, NULL) < 0) {
		russ_standard_answer_handler(self);
		russ_sconn_fatal(self, RUSS_MSG_NO_SWITCH_USER, RUSS_EXIT_FAILURE);
		return -1;
	}

	/* switch user, dial next service, and splice */
	if (((cconn = russ_dialv(deadline, req->op, req->spath, req->attrv, req->argv)) == NULL)
		|| (russ_sconn_splice(self, cconn) < 0)) {
		russ_cconn_close(cconn);
		russ_standard_answer_handler(self);
		russ_sconn_fatal(self, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		return -1;
	}
	return 0;
}
