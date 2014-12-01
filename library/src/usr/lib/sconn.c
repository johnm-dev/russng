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
* Accept dial.
*
* @param deadline	deadline to complete operation
* @param lisd		listen() socket descriptor
* @return		new server connection object with
*			credentials (not fully established); NULL on
*			failure
*/
struct russ_sconn *
russ_sconn_accept(russ_deadline deadline, int lisd) {
	struct russ_sconn	*self;
	struct sockaddr_un	servaddr;
	socklen_t		servaddr_len;

	if ((lisd < 0)
		|| ((self = russ_sconn_new()) == NULL)) {
		return NULL;
	}

	servaddr_len = sizeof(struct sockaddr_un);
	if ((self->sd = russ_accept_deadline(deadline, lisd, (struct sockaddr *)&servaddr, &servaddr_len)) < 0) {
		fprintf(stderr, "warning: russ_sconn_accept() fails with errno (%d)\n", errno);
		goto free_sconn;
	}
	if (russ_get_creds(self->sd, &(self->creds)) < 0) {
		fprintf(stderr, "warning: russ_get_creds() fails\n");
		goto close_sd;
	}
	return self;

close_sd:
	russ_fds_close(&self->sd, 1);
free_sconn:
	self = russ_free(self);
	return NULL;
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
russ_sconn_send_fds(struct russ_sconn *self, int nfds, int *cfds) {
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
		if (russ_send_fd(self->sd, cfds[i]) < 0) {
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
* @param self		server connection object
* @param nfds		number of elements in cfds (and sfds) array
* @param cfds		array of descriptors to send to client
*			(corresponding to the server-side connection
*			fds)
* @return		0 on success; -1 on error
*/
int
russ_sconn_answer(struct russ_sconn *self, int nfds, int *cfds) {
	int	csysfds[RUSS_CONN_NSYSFDS];
	int	i;

	if (nfds < 0) {
		return -1;
	}

	/* set up system fds */
	russ_fds_init(csysfds, RUSS_CONN_NSYSFDS, -1);
	russ_fds_init(self->sysfds, RUSS_CONN_NSYSFDS, -1);
	if (russ_make_pipes(RUSS_CONN_NSYSFDS, csysfds, self->sysfds) < 0) {
		fprintf(stderr, "error: cannot create pipes\n");
		return -1;
	}

	if ((russ_sconn_send_fds(self, RUSS_CONN_NSYSFDS, csysfds) < 0)
		|| (russ_sconn_send_fds(self, nfds, cfds) < 0)) {
		russ_fds_close(csysfds, RUSS_CONN_NSYSFDS);
		russ_fds_close(self->sysfds, RUSS_CONN_NSYSFDS);
		russ_fds_close(&self->sd, 1);
		return -1;
	}
	russ_fds_close(&self->sd, 1);
	return 0;
}

/**
* Default answer handler which sets up standard fds (stdin, stdout,
* stderr) and answers the request.
*
* @param self		server connection object
* @return		0 on success; -1 on failure
*/
int
russ_sconn_answerhandler(struct russ_sconn *self) {
	int	cfds[RUSS_CONN_NFDS];
	int	tmpfd;

	russ_fds_init(cfds, RUSS_CONN_NFDS, -1);
	russ_fds_init(self->fds, RUSS_CONN_NFDS, -1);
	if (russ_make_pipes(RUSS_CONN_STD_NFDS, cfds, self->fds) < 0) {
		fprintf(stderr, "error: cannot create pipes\n");
		return -1;
	}
	/* swap fds for stdin */
	tmpfd = cfds[0];
	cfds[0] = self->fds[0];
	self->fds[0] = tmpfd;

	if (russ_sconn_answer(self, RUSS_CONN_STD_NFDS, cfds) < 0) {
		russ_fds_close(cfds, RUSS_CONN_STD_NFDS);
		russ_fds_close(self->fds, RUSS_CONN_STD_NFDS);
		return -1;
	}
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
	if ((russ_sconn_send_fds(self, RUSS_CONN_NSYSFDS, dconn->sysfds) < 0)
		|| (russ_sconn_send_fds(self, RUSS_CONN_NFDS, dconn->fds) < 0)) {
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
russ_sconn_await_req(struct russ_sconn *self, russ_deadline deadline) {
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
	russ_fds_close(self->sysfds, RUSS_CONN_NSYSFDS);
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
* @param exitst		exit status
* @return		0 on success; -1 on failure
*/
int
russ_sconn_exit(struct russ_sconn *self, int exitst) {
	char	buf[32], *bp;

	/* close all non-sysfds */
	russ_fds_close(self->fds, RUSS_CONN_NFDS);

	/* send exit status */
	bp = buf;
	if ((self->sysfds[RUSS_CONN_SYSFD_EXIT] < 0)
		|| ((bp = russ_enc_exit(bp, bp+sizeof(buf), exitst)) == NULL)
		|| (russ_writen(self->sysfds[RUSS_CONN_SYSFD_EXIT], buf, bp-buf) < bp-buf)) {
		return -1;
	}

	/* last step: close exit fd */
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
* @param exitst		exit status
* @return		0 on success; -1 on failure (but the server
*			connection object is closed)
*/
int
russ_sconn_fatal(struct russ_sconn *self, const char *msg, int exitst) {
	russ_dprintf(self->fds[2], "%s\n", msg);
	return russ_sconn_exit(self, exitst);
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
russ_sconn_redialandsplice(struct russ_sconn *self, russ_deadline deadline, struct russ_req *req) {
	struct russ_cconn	*cconn;

	/* switch user */
	if (russ_switch_user(self->creds.uid, self->creds.gid, 0, NULL) < 0) {
		russ_sconn_answerhandler(self);
		russ_sconn_fatal(self, RUSS_MSG_NOSWITCHUSER, RUSS_EXIT_FAILURE);
		return -1;
	}

	/* switch user, dial next service, and splice */
	if (((cconn = russ_dialv(deadline, req->op, req->spath, req->attrv, req->argv)) == NULL)
		|| (russ_sconn_splice(self, cconn) < 0)) {
		russ_cconn_close(cconn);
		russ_sconn_answerhandler(self);
		russ_sconn_fatal(self, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		return -1;
	}
	return 0;
}
