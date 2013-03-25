/*
** lib/srv.c
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

#include "russ.h"

/**
* Create new service_ ode object.
*
* @param name		node name
* @param handler	service handler
* @return		service node object
*/
struct russ_svc_node *
russ_svc_node_new(char *name, russ_svc_handler handler) {
	struct russ_svc_node	*self;

	if ((self = malloc(sizeof(struct russ_svc_node))) == NULL) {
		return NULL;
	}
	if ((self->name = strdup(name)) == NULL) {
		goto free_node;
	}
	self->handler = handler;
	self->next = NULL;
	self->children = NULL;
	return self;
free_node:
	free(self);
	return NULL;
}

/**
* Free service node.
*
* @param self		service node object
* @return		NULL
*/
struct russ_svc_node *
russ_svc_node_free(struct russ_svc_node *self) {
	if (self != NULL) {
		free(self->name);
		free(self);
	}
	return NULL;
}

/**
* Add child service node.
*
* @param self		service node object
* @param name		service name
* @param handler	service handler
* @return		child service node object; NULL on failure
*/
struct russ_svc_node *
russ_svc_node_add(struct russ_svc_node *self, char *name, russ_svc_handler handler) {
	struct russ_svc_node	*curr, *last, *node;
	int				cmp;

	last = NULL;
	curr = self->children;
	while (curr != NULL) {
		if ((cmp = strcmp(curr->name, name)) == 0) {
			/* already exists */
			return NULL;
		} else if (cmp > 0) {
			break;
		}
		last = curr;
		curr = curr->next;
	}
	if ((node = russ_svc_node_new(name, handler)) == NULL) {
		return NULL;
	}
	if (last == NULL) {
		self->children = node;
		node->next = NULL;
	} else {
		last->next = node;
		node->next = curr;
	}

	return node;
}

/**
* Find node matching path starting at a node.
*
* @param self		service node object starting point
* @param path		path to match (relative to self)
* @return		matching service node object; NULL on failure
*/
struct russ_svc_node *
russ_svc_node_find(struct russ_svc_node *self, char *path) {
	struct russ_svc_node	*node;
	char			*sep;
	int			len, cmp;

	if ((self->virtual) || (strcmp(path, "") == 0)) {
		return self;
	}

	if ((sep = strchr(path, '/')) == NULL) {
		sep = strchr(path, '\0');
	}
	len = sep-path;
	for (node = self->children; node != NULL; node = node->next) {
		if ((cmp = strncmp(node->name, path, len)) > 0) {
			node = NULL;
			break;
		} else if ((cmp == 0) && (node->name[len] == '\0')) {
			if (*sep != '\0') {
				node = russ_svc_node_find(node, &path[len+1]);
			}
			break;
		}
	}
	return node;
}

int
russ_svc_node_set_virtual(struct russ_svc_node *self, int value) {
	self->virtual = value;
	return 1;
}

struct russ_svr *
russ_svr_new(struct russ_svc_node *root, int type) {
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

	return self;
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
	struct russ_req		*req;
	struct russ_svc_node	*node;

	if (russ_conn_await_request(conn, RUSS_DEADLINE_NEVER) < 0) {
		/* failure */
		goto cleanup;
	}

	/* validate spath: must be absolute */
	req = &(conn->req);
	if ((req->spath[0] != '/') || (req->spath[0] == '\0')) {
			/* invalid spath */
			goto cleanup;
	}

	if ((node = russ_svc_node_find(self->root, &(req->spath[1]))) == NULL) {
		/* TODO: how to handle this in general?
		** for HELP, LIST, EXECUTE? other? under what conditions
		** is there a stdout and exit fd?
		*/
		russ_standard_answer_handler(conn);
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		goto cleanup;
	}

	if (0) {
		/* TODO: add support for non-standard answer handler */
		if (node->handler) {
			node->handler(conn);
		} else {
			russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		}
	} else if (russ_standard_answer_handler(conn) < 0) {
		goto cleanup;
	}

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
		node->handler(conn);
	} else {
		russ_conn_fatal(conn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}

cleanup:
	/* clean up */
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
		if ((conn = russ_standard_accept_handler(self->lis, RUSS_DEADLINE_NEVER)) == NULL) {
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
		;
	}
}