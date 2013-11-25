/*
** lib/cconn.c
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
* Free client connection object.
*
* @param self		client connection object
* @return		NULL value
*/
struct russ_cconn *
russ_cconn_free(struct russ_cconn *self) {
	free(self);
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
	struct russ_cconn	*cconn;

	if ((cconn = malloc(sizeof(struct russ_cconn))) == NULL) {
		return NULL;
	}
	cconn->sd = -1;
	russ_fds_init(cconn->fds, RUSS_CONN_NFDS, -1);
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
* @return		0 on success; -1 on error
*/
static int
russ_cconn_recvfds(struct russ_cconn *self, russ_deadline deadline) {
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
* Close connection.
*
* All connection fds are closed and the client connection object
* updated.
*
* @param self		client connection object
*/
void
russ_cconn_close(struct russ_cconn *self) {
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
* @param[out] exit_status
			exit status
* @param deadline	deadline to wait
* @return		0 on success; on -1 general failure; -2 on exit fd closed; -3 on deadline expired
*/
int
russ_cconn_wait(struct russ_cconn *self, russ_deadline deadline, int *exit_status) {
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
russ_cconn_send_request(struct russ_cconn *self, russ_deadline deadline, struct russ_req *req) {
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
russ_dialv(russ_deadline deadline, char *op, char *spath, char **attrv, char **argv) {
	struct russ_cconn	*cconn;
	struct russ_req		*req;
	struct russ_target	*targ;
	char			*saddr, *spath2;

	if (russ_spath_split(spath, &saddr, &spath2) < 0) {
		return NULL;
	}
	if (((cconn = russ_cconn_new()) == NULL)
		|| ((cconn->sd = russ_connectunix_deadline(deadline, saddr)) < 0)) {
		goto free_saddr;
	}

	if (((req = russ_req_new(RUSS_REQ_PROTOCOL_STRING, op, spath2, attrv, argv)) == NULL)
		|| (russ_cconn_send_request(cconn, deadline, req) < 0)
		|| (russ_cconn_recvfds(cconn, deadline) < 0)) {
		goto free_request;
	}
	free(saddr);
	free(spath2);
	russ_fds_close(&cconn->sd, 1);	/* sd not needed anymore */
	russ_req_free(req);
	return cconn;

free_request:
	russ_req_free(req);
close_cconn:
	russ_cconn_close(cconn);
	free(cconn);
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
* @return		client connection object, NULL on failure
*/
struct russ_cconn *
russ_diall(russ_deadline deadline, char *op, char *spath, char **attrv, ...) {
	struct russ_cconn	*cconn;
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

	cconn = russ_dialv(deadline, op, spath, attrv, argv);
	free(argv);

	return cconn;
}