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
* @param self		server connection object
* @param nfds		number of fds to send (from index 0)
* @param cfds		client-side descriptors
* @param sfds		server-side descriptors
* @return		0 on success; -1 on error
*/
int
russ_sconn_sendfds(struct russ_sconn *self, int nfds, int *cfds, int *sfds) {
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
* @param self		accepted server connection object
* @param nfds		number of elements in cfds (and sfds) array
* @param cfds		array of descriptors to send to client
* @param sfds		array of descriptors for server side
* @return		0 on success; -1 on error
*/
int
russ_sconn_answer(struct russ_sconn *self, int nfds, int *cfds, int *sfds) {
	if ((nfds < 0) || (russ_sconn_sendfds(self, nfds, cfds, sfds) < 0)) {
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
* @param self		server connection object
* @param dconn		dialed client connection object
*/
int
russ_sconn_splice(struct russ_sconn *self, struct russ_cconn *dconn) {
	int	cfds[RUSS_CONN_NFDS];
	int	i, ev;

	for (i = 0; i < RUSS_CONN_NFDS; i++) {
		cfds[i] = dconn->fds[i];
	}
	ev = russ_sconn_sendfds(self, RUSS_CONN_NFDS, cfds, NULL);

	/* dconn fds are closed by russ_sconn_sendfds */
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
	char	buf[1024];
	char	*exit_string = "";
	char	*bp, *bend;

	if (self->fds[3] < 0) {
		return -1;
	}
	bp = buf;
	bend = bp+sizeof(buf);
	if (((bp = russ_enc_exit(bp, bend, exit_status)) == NULL)
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
* An error message is sent to the connection exit fd (with a
* trailing newline) and the exit_status over the exit fd. If the
* exit fd is already closed, then no message is written or exit
* status sent.
*
* @param self		server connection object
* @param msg		message string (no newline)
* @param exit_status	exit status
* @return		0 on success; -1 on failure
*/
int
russ_sconn_exits(struct russ_sconn *self, const char *msg, int exit_status) {
	if (self->fds[3] < 0) {
		return -1;
	}
	russ_dprintf(self->fds[2], "%s\n", msg);
	return russ_sconn_exit(self, exit_status);
}

/**
* Helper routine to write final message, write exit status, and
* close connection fds.
*
* Calls russ_sconn_exits() followed by russ_sconn_close().
*
* @param self		server connection object
* @param msg		message string (no newline)
* @param exit_status	exit status
* @return		0 on success; -1 on failure (but the
*			server connection object is closed)
*/
int
russ_sconn_fatal(struct russ_sconn *self, const char *msg, int exit_status) {
	int	ev;

	ev = russ_sconn_exits(self, msg, exit_status);
	russ_sconn_close(self);
	return ev;
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
