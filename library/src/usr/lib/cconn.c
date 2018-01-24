/*
* lib/cconn.c
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

#include <fcntl.h> // ruspawn()
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <libgen.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ/priv.h"

/**
* Free client connection object.
*
* @param self		client connection object
* @return		NULL value
*/
struct russ_cconn *
russ_cconn_free(struct russ_cconn *self) {
	self = russ_free(self);
	return NULL;
}

/**
* Create and initialize a client connection object.
*
* @return		a new, initialized client connection object;
*			NULL on failure
*/
struct russ_cconn *
russ_cconn_new(void) {
	struct russ_cconn	*cconn = NULL;

	if ((cconn = russ_malloc(sizeof(struct russ_cconn))) == NULL) {
		return NULL;
	}
	cconn->sd = -1;
	russ_fds_init(cconn->fds, RUSS_CONN_NFDS, -1);
	cconn->nevbuf = 0;
	return cconn;
}

/**
* Close a descriptor of the connection.
*
* @param self		client connection object
* @param index		index of the descriptor
*/
void
russ_cconn_close_fd(struct russ_cconn *self, int index) {
	russ_fds_close(&(self->fds[index]), 1);
}

/**
* Receive fds from client connection.
*
* @param self		client connection object
* @param deadline	deadline for operation
* @param nfds		size of fds array
* @param fds		array for descriptors
* @return		0 on success; -1 on error
*/
static int
russ_cconn_recv_fds(struct russ_cconn *self, russ_deadline deadline, int nfds, int *fds) {
	char	buf[32+RUSS_CONN_MAX_NFDS];
	char	*bp = NULL, *bend = NULL;
	int	recvnfds, i;

	/* recv count of fds and fd statuses */
	if ((russ_readn_deadline(deadline, self->sd, buf, 4) < 4)
		|| (russ_dec_int32(buf, &recvnfds) == NULL)
		|| (recvnfds > nfds)
		|| (russ_readn_deadline(deadline, self->sd, buf, recvnfds) < recvnfds)) {
		return -1;
	}

	/* (assume initialization) recv fds and load */
	for (i = 0; i < recvnfds; i++) {
		if ((buf[i]) && (russ_recv_fd(self->sd, &fds[i]) < 0)) {
			return -1;
		}
	}
	return 0;
}

/**
* Close connection.
*
* All connection fds are closed and the client connection object
* updated.
*
* @param self		client connection object
*/
void
russ_cconn_close(struct russ_cconn *self) {
	russ_fds_close(self->sysfds, RUSS_CONN_NSYSFDS);
	russ_fds_close(self->fds, RUSS_CONN_NFDS);
	russ_fds_close(&self->sd, 1);
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
* @param self		client connection object
* @param[out] exitst	exit status
* @param deadline	deadline to wait
* @return		0 on success; on -1 general failure; -2 on exit fd closed; -3 on deadline expired
*/
int
russ_cconn_wait(struct russ_cconn *self, russ_deadline deadline, int *exitst) {
	struct pollfd	poll_fds[1];
	int		rv, _exitst;

	if (self->sysfds[RUSS_CONN_SYSFD_EXIT] < 0) {
		if (self->nevbuf == 4) {
			goto set_exitst;
		}
		return RUSS_WAIT_BADFD;
	}

	poll_fds[0].fd = self->sysfds[RUSS_CONN_SYSFD_EXIT];
	poll_fds[0].events = POLLIN;
	while (1) {
		rv = russ_poll_deadline(deadline, poll_fds, 1);
		if (rv == 0) {
			/* timeout */
			return RUSS_WAIT_TIMEOUT;
		} else if (rv < 0) {
			return RUSS_WAIT_FAILURE;
		} else {
			if (poll_fds[0].revents & POLLIN) {
				// TODO: should this be a byte or integer?
				rv = russ_read(self->sysfds[RUSS_CONN_SYSFD_EXIT],
					&self->evbuf[self->nevbuf], 4-self->nevbuf);
				if (rv < 0) {
					/* serious error; close fd? */
					return RUSS_WAIT_FAILURE;
				}
				self->nevbuf += rv;
				if (self->nevbuf == 4) {
					break;
				}
			} else if (poll_fds[0].revents & POLLHUP) {
				return RUSS_WAIT_HUP;
			}
		}
	}
	russ_fds_close(&self->sysfds[RUSS_CONN_SYSFD_EXIT], 1);

set_exitst:
	russ_dec_exit(self->evbuf, &_exitst);
	if (exitst) {
		*exitst = _exitst;
	}
	return RUSS_WAIT_OK;
}

/**
* Send request.
*
* Request information is encoded and sent over the connection.
*
* @param self		client connection object
* @param deadline	deadline to send
* @param req		request object
* @return		0 on success; -1 on error
*/
int
russ_cconn_send_req(struct russ_cconn *self, russ_deadline deadline, struct russ_req *req) {
	char	buf[RUSS_REQ_BUF_MAX];
	char	*bp = NULL;

	if ((req == NULL)
		|| ((bp = russ_enc_req(buf, buf+sizeof(buf), req)) == NULL)
		|| (russ_writen_deadline(deadline, self->sd, buf, bp-buf) < bp-buf)) {
		return -1;
	}
	return 0;
}

/**
* Dial service.
*
* Connect to a service, send request information, and get fds.
* Received fds are saved to the client connection object.
*
* @param deadline	deadline to complete operation
* @param op		operation string
* @param spath		service path
* @param attrv		NULL-terminated array of attributes ("name=value" strings)
* @param argv		NULL-terminated array of arguments
* @return		client connection object; NULL on failure
*/
struct russ_cconn *
russ_dialv(russ_deadline deadline, const char *op, const char *spath, char **attrv, char **argv) {
	struct russ_cconn	*cconn = NULL;
	struct russ_req		*req = NULL;
	struct russ_target	*targ = NULL;
	char			*caddr = NULL;
	char			*saddr = NULL, *spath2 = NULL;

	if (russ_spath_split(spath, &saddr, &spath2) < 0) {
		return NULL;
	}
	if (russ_is_conffile(saddr)) {
		/* saddr points to configuration */
		caddr = realpath(saddr, NULL);
		saddr = russ_free(saddr);

		/* use saddr to point to spawned socket */
		saddr = russ_ruspawn(caddr);
		caddr = russ_free(caddr);
		if (saddr == NULL) {
			goto free_saddr;
		}
	}

	if ((cconn = russ_cconn_new()) == NULL) {
		goto free_saddr;
	}
	if ((cconn->sd = russ_connectunix_deadline(deadline, saddr)) < 0) {
		goto close_cconn;
	}

	russ_fds_init(cconn->sysfds, RUSS_CONN_NSYSFDS, -1);
	russ_fds_init(cconn->fds, RUSS_CONN_NFDS, -1);

	if ((req = russ_req_new(RUSS_REQ_PROTOCOLSTRING, op, spath2, attrv, argv)) == NULL) {
		goto close_cconn;
	}
	if ((russ_cconn_send_req(cconn, deadline, req) < 0)
		|| (russ_cconn_recv_fds(cconn, deadline, RUSS_CONN_NSYSFDS, cconn->sysfds) < 0)
		|| (russ_cconn_recv_fds(cconn, deadline, RUSS_CONN_NFDS, cconn->fds) < 0)) {
		goto free_request;
	}
	saddr = russ_free(saddr);
	spath2 = russ_free(spath2);
	russ_fds_close(&cconn->sd, 1);	/* sd not needed anymore */
	russ_req_free(req);
	return cconn;

free_request:
	russ_req_free(req);
close_cconn:
	russ_cconn_close(cconn);
	cconn = russ_free(cconn);
free_saddr:
	saddr = russ_free(saddr);
	spath2 = russ_free(spath2);
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
* @return		client connection object, NULL on failure
*/
struct russ_cconn *
russ_diall(russ_deadline deadline, const char *op, const char *spath, char **attrv, ...) {
	struct russ_cconn	*cconn = NULL;
	va_list			ap;
	void			*p = NULL;
	int			i, argc;
	char			**argv = NULL;

	/* count args */
	va_start(ap, attrv);
	for (argc = 0; argc < RUSS_REQ_ARGS_MAX; argc++) {
		if ((p = va_arg(ap, char *)) == NULL) {
			break;
		}
	}
	va_end(ap);

	/* create argv */
	if ((argv = russ_malloc(sizeof(char *)*argc)) == NULL) {
		return NULL;
	}
	va_start(ap, attrv);
	for (i = 0; i < argc; i++) {
		argv[i] = va_arg(ap, char *);
	}
	va_end(ap);

	cconn = russ_dialv(deadline, op, spath, attrv, argv);
	argv = russ_free(argv);

	return cconn;
}
