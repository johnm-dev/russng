/*
** lib/server.c
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
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "russ_priv.h"

struct russ_svr *
russ_svr_new(struct russ_svcnode *root, int type) {
	struct russ_svr	*self;

	if ((self = malloc(sizeof(struct russ_svr))) == NULL) {
		return NULL;
	}
	self->root = root;
	self->type = type;
	self->saddr = NULL;
	self->mode = 0;
	self->uid = -1;
	self->gid = -1;
	self->lis = NULL;
	self->accept_handler = russ_standard_accept_handler;
	self->accept_timeout = RUSS_SVR_TIMEOUT_ACCEPT;
	self->await_timeout = RUSS_SVR_TIMEOUT_AWAIT;
	self->auto_switch_user = 0;

	return self;
}

/**
* Accept connection from server listen socket.
*
* Wrapper for calling registered accept handler.
*
* @param self		server object
* @param deadline	deadline to complete operation
*/
struct russ_conn *
russ_svr_accept(struct russ_svr *self, russ_deadline deadline) {
	return self->accept_handler(self->lis, deadline);
}

struct russ_lis *
russ_svr_announce(struct russ_svr *self, char *saddr, mode_t mode, uid_t uid, gid_t gid) {
	if ((self->saddr = strdup(saddr)) == NULL) {
		return;
	}
	self->mode = mode;
	self->uid = uid;
	self->gid = gid;
	if ((self->lis = russ_announce(self->saddr, self->mode, self->uid, self->gid)) == NULL) {
		goto free_saddr;
	}
	return self->lis;
free_saddr:
	free(self->saddr);
	self->saddr = NULL;
	return NULL;
}

/**
* Register an alternative accept handler.
*
* @param self		russ server object
* @param handler	accept handler
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_accept_handler(struct russ_svr *self, russ_accept_handler handler) {
	if (handler == NULL) {
		return -1;
	}
	self->accept_handler = handler;
	return 0;
}

/**
* Set to perform (or not) auto switch user.
*
* If enabled, the server will automatically call russ_switch_user()
* to change (forked) process uid/gid to that of credentials.
*
* @param self		russ server object
* @param value		0 to disable; 1 to enable
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_auto_switch_user(struct russ_svr *self, int value) {
	self->auto_switch_user = value;
	return 0;
}

/**
* Find service handler and it invoke it.
*
* Special cases:
* opnum == RUSS_OPNUM_HELP - fallback to spath == "/" if available
* opnum == RUSS_OPNUM_LIST - list node->children if found
*
* Service handlers are expected to call russ_conn_exit() before
* returning. As a failsafe procedure, exit codes are sent back
* if the exit fd is open, all connection fds are closed.
*
* @param self		server object
* @param conn		connection object
*/
void
russ_svr_handler(struct russ_svr *self, struct russ_conn *conn) {
	struct russ_sess	sess;
	struct russ_req		*req;
	struct russ_svcnode	*node;

	if ((req = russ_conn_await_request(conn, russ_to_deadline(self->await_timeout))) == NULL) {
		/* failure */
		goto cleanup;
	}

	/* validate opnum */
	if (req->opnum == RUSS_OPNUM_NOT_SET) {
		/* invalid opnum */
		russ_conn_fatal(conn, RUSS_MSG_BAD_OP, RUSS_EXIT_SYS_FAILURE);
		goto cleanup;
	}
	/* validate spath: must be absolute */
	if ((req->spath[0] != '/') || (req->spath[0] == '\0')) {
		/* invalid spath */
		goto cleanup;
	}

	if ((node = russ_svcnode_find(self->root, &(req->spath[1]))) == NULL) {
		/* TODO: how to handle this in general?
		** for HELP, LIST, EXECUTE? other? under what conditions
		** is there a stdout and exit fd?
		*/
		russ_standard_answer_handler(conn);
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		goto cleanup;
	}

	if ((node->auto_answer) && (russ_standard_answer_handler(conn) < 0)) {
		goto cleanup;
	}

	/* auto switch user if requested */
	if (self->auto_switch_user) {
		if (russ_switch_user(conn->creds.uid, conn->creds.gid, 0, NULL) < 0) {
			russ_conn_fatal(conn, RUSS_MSG_NO_SWITCH_USER, RUSS_EXIT_FAILURE);
			goto cleanup;
		}
	}

	/* prepare session object */
	sess.conn = conn;
	sess.svr = self;
	sess.spath[0] = '\0';
	sess.req = req;

	/* virtual node */
	if (node->virtual) {
		goto call_node_handler;
	}

	/* default handling; non-virtual node */
	switch (req->opnum) {
	case RUSS_OPNUM_LIST:
		/* TODO: test against ctxt.spath */
		for (node = node->children; node != NULL; node = node->next) {
			russ_dprintf(conn->fds[1], "%s\n", node->name);
		}
		russ_conn_exit(conn, RUSS_EXIT_SUCCESS);
		break;
	case RUSS_OPNUM_HELP:
		node = self->root;
		/* TODO: test against ctxt.spath */
		/* fall through */
		break;
	}

call_node_handler:
	if (node->handler) {
		node->handler(&sess);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}

cleanup:
	/* clean up: on error and fallthrough on success */
	if (req != NULL) {
		req = russ_req_free(req);
	}
	russ_conn_exit(conn, RUSS_EXIT_FAILURE);
	russ_conn_close(conn);
}

/**
* Server loop for forking servers.
*
* @param self		server object
*/
void
russ_svr_loop_fork(struct russ_svr *self) {
	struct russ_conn	*conn;
	pid_t			pid;

	while (1) {
		if ((conn = self->accept_handler(self->lis, russ_to_deadline(self->accept_timeout))) == NULL) {
			fprintf(stderr, "error: cannot accept connection\n");
			continue;
		}
		if ((pid = fork()) == 0) {
			setsid();
			russ_lis_close(self->lis);
			self->lis = russ_lis_free(self->lis);
			if (fork() == 0) {
				russ_svr_handler(self, conn);

				/* failsafe exit info (if not provided) */
				russ_conn_fatal(conn, RUSS_MSG_NO_EXIT, RUSS_EXIT_SYS_FAILURE);
				conn = russ_conn_free(conn);
				exit(0);
			}
			exit(0);
		}
		russ_conn_close(conn);
		conn = russ_conn_free(conn);
		waitpid(pid, 0);
	}
}

struct helper_data {
	struct russ_svr		*svr;
	struct russ_conn 	*conn;
};

/**
* Helper for threaded servers.
*
* Takes care of calling the russ_svr_handler, failsafe exit, and
* freeing objects as needed.
*
* @param data		helper_data object
*/
static void
*russ_svr_handler_helper(void *data) {
	struct russ_svr		*svr = ((struct helper_data *)data)->svr;
	struct russ_conn	*conn = ((struct helper_data *)data)->conn;

	russ_svr_handler(svr, conn);

	/* failsafe exit info (if not provided) */
	russ_conn_fatal(conn, RUSS_MSG_NO_EXIT, RUSS_EXIT_SYS_FAILURE);

	/* free objects */
	conn = russ_conn_free(conn);
	free(data);
	data = NULL;

	pthread_exit(NULL);
}

/**
* Server loop for threaded servers.
*
* Calls helper to simplify argument passing, object creation (data
* and conn here) and freeing (in helper) and closing (in helper).
*
* @param self		server object
*/
void
russ_svr_loop_thread(struct russ_svr *self) {
	struct russ_conn	*conn;
	struct helper_data	*data;

	while (1) {
		if (((conn = self->accept_handler(self->lis, russ_to_deadline(self->accept_timeout))) == NULL)
			|| ((data = malloc(sizeof(struct helper_data))) != NULL)) {
			if (conn) {
				russ_conn_fatal(conn, RUSS_MSG_NO_EXIT, RUSS_EXIT_SYS_FAILURE);
				russ_conn_free(conn);
			}
			fprintf(stderr, "error: cannot accept connection\n");
			continue;
		}
		data->svr = self;
		data->conn = conn;
		if (pthread_create(NULL, NULL, russ_svr_handler_helper, data) < 0) {
			fprintf(stderr, "error: cannot spawn thread\n");
		}
	}
}

/**
* Dispatches to specific server loop by server type.
*
* @param self		server object
*/
void
russ_svr_loop(struct russ_svr *self) {
	if (self->type == RUSS_SVR_TYPE_FORK) {
		russ_svr_loop_fork(self);
	} else if (self->type == RUSS_SVR_TYPE_THREAD) {
		russ_svr_loop_thread(self);
	}
}