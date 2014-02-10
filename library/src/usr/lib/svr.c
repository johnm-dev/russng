/*
** lib/svr.c
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
#include <sys/wait.h>

#include "russ_priv.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX	64
#endif /* HOST_NAME_MAX */

struct russ_svr *
russ_svr_new(struct russ_svcnode *root, int type) {
	struct russ_svr	*self;

	if ((self = malloc(sizeof(struct russ_svr))) == NULL) {
		return NULL;
	}
	self->root = root;
	self->type = type;
	self->mpid = getpid();
	self->ctime = russ_gettime();
	self->saddr = NULL;
	self->mode = 0;
	self->uid = -1;
	self->gid = -1;
	self->lis = NULL;
	self->accept_handler = russ_standard_accept_handler;
	self->accept_timeout = RUSS_SVR_TIMEOUT_ACCEPT;
	self->answer_handler = russ_standard_answer_handler;
	self->await_timeout = RUSS_SVR_TIMEOUT_AWAIT;
	self->auto_switch_user = 0;
	self->help = NULL;

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
struct russ_sconn *
russ_svr_accept(struct russ_svr *self, russ_deadline deadline) {
	return self->accept_handler(self->lis, deadline);
}

struct russ_lis *
russ_svr_announce(struct russ_svr *self, char *saddr, mode_t mode, uid_t uid, gid_t gid) {
	if (self == NULL) {
		return NULL;
	}
	if ((self->saddr = strdup(saddr)) == NULL) {
		return NULL;
	}
	self->mode = mode;
	self->uid = uid;
	self->gid = gid;
	if ((self->lis = russ_announce(self->saddr, self->mode, self->uid, self->gid)) == NULL) {
		goto free_saddr;
	}
	return self->lis;
free_saddr:
	self->saddr = russ_free(self->saddr);
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
russ_svr_set_accepthandler(struct russ_svr *self, russ_accepthandler handler) {
	if (self == NULL) {
		return -1;
	}
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
	if (self == NULL) {
		return -1;
	}
	self->auto_switch_user = value;
	return 0;
}

/**
* Set (make copy) the server help string.
*
* @param self		server object
* @param help		help string
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_help(struct russ_svr *self, char *help) {
	if (self == NULL) {
		return -1;
	}
	if ((self->help = strdup(help)) == NULL) {
		return -1;
	}
	return 0;
}

/**
* Find service handler and it invoke it.
*
* Special cases:
* opnum == RUSS_OPNUM_HELP - fallback to spath == "/" if available
* opnum == RUSS_OPNUM_LIST - list node->children if found
*
* Service handlers are expected to call russ_sconn_exit() before
* returning. As a failsafe procedure, exit codes are sent back
* if the exit fd is open, all connection fds are closed.
*
* @param self		server object
* @param sconn		server connection object
*/
void
russ_svr_handler(struct russ_svr *self, struct russ_sconn *sconn) {
	struct russ_sess	sess;
	struct russ_req		*req;
	struct russ_svcnode	*node;

	if (self == NULL) {
		return;
	}

	if ((req = russ_sconn_await_request(sconn, russ_to_deadline(self->await_timeout))) == NULL) {
		/* failure */
		goto cleanup;
	}

	/* validate opnum */
	if (req->opnum == RUSS_OPNUM_NOT_SET) {
		/* invalid opnum */
		russ_sconn_fatal(sconn, RUSS_MSG_BAD_OP, RUSS_EXIT_SYS_FAILURE);
		goto cleanup;
	}
	/* validate spath: must be absolute */
	if ((req->spath[0] != '/') || (req->spath[0] == '\0')) {
		/* invalid spath */
		goto cleanup;
	}

	if ((node = russ_svcnode_find(self->root, &(req->spath[1]), sess.spath, sizeof(sess.spath))) == NULL) {
		/* we need standard fds */
		russ_standard_answer_handler(sconn);
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
		goto cleanup;
	}

	if ((node->auto_answer)
		&& ((self->answer_handler == NULL) || (self->answer_handler(sconn) < 0))) {
		goto cleanup;
	}

	/* auto switch user if requested */
	if (self->auto_switch_user) {
		if (russ_switch_user(sconn->creds.uid, sconn->creds.gid, 0, NULL) < 0) {
			russ_sconn_fatal(sconn, RUSS_MSG_NO_SWITCH_USER, RUSS_EXIT_FAILURE);
			goto cleanup;
		}
	}

	/* prepare session object */
	sess.sconn = sconn;
	sess.svr = self;
	sess.spath[0] = '\0';
	sess.req = req;

	/* call handler, if available */
	if ((node) && (node->handler)) {
		node->handler(&sess);
	} else {
		russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	}

	/*
	* fallback handler:
	* * when no handler
	* * when request is unserviced
	* * when exit() is not called by handler
	*/
	switch (req->opnum) {
	case RUSS_OPNUM_LIST:
		/* TODO: test against ctxt.spath */
		if (!node->virtual) {
			if (node->children != NULL) {
				for (node = node->children; node != NULL; node = node->next) {
					russ_dprintf(sconn->fds[1], "%s\n", node->name);
				}
				russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
			} else if (node->wildcard) {
				russ_sconn_fatal(sconn, RUSS_MSG_NO_LIST, RUSS_EXIT_SUCCESS);
			}
		}
		break;
	case RUSS_OPNUM_HELP:
		if (self->help != NULL) {
			russ_dprintf(sconn->fds[1], self->help);
			russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		} else {
			node = self->root;
		}
		break;
	case RUSS_OPNUM_INFO:
		if (sconn->creds.uid == self->uid) {
			char	hostname[HOST_NAME_MAX];

			gethostname(hostname, sizeof(hostname));
			russ_dprintf(sconn->fds[1], "hostname=%s\n", hostname);
			russ_dprintf(sconn->fds[1], "saddr=%s\n", self->saddr);
			russ_dprintf(sconn->fds[1], "mpid=%d\n", self->mpid);
			russ_dprintf(sconn->fds[1], "ctime=%ld\n", self->ctime);
			russ_dprintf(sconn->fds[1], "pid=%d\n", getpid());
			russ_dprintf(sconn->fds[1], "time=%ld\n", russ_gettime());
		}
		russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
		break;
	default:
		russ_sconn_fatal(sconn, RUSS_MSG_BAD_OP, RUSS_EXIT_FAILURE);
	}

cleanup:
	/* clean up: on error and fallthrough on success */
	if (req != NULL) {
		req = russ_req_free(req);
	}
	russ_sconn_fatal(sconn, RUSS_MSG_NO_SERVICE, RUSS_EXIT_FAILURE);
	russ_sconn_close(sconn);
}

/**
* Server loop for forking servers.
*
* @param self		server object
*/
void
russ_svr_loop_fork(struct russ_svr *self) {
	struct russ_sconn	*sconn;
	pid_t			pid, wpid;
	int			wst;

	if (self == NULL) {
		return;
	}

	while (1) {
		if ((sconn = self->accept_handler(self->lis, russ_to_deadline(self->accept_timeout))) == NULL) {
			fprintf(stderr, "error: cannot accept connection\n");
			sleep(1);
			continue;
		}
		if ((pid = fork()) == 0) {
			setsid();
			signal(SIGHUP, SIG_IGN);

			russ_lis_close(self->lis);
			self->lis = russ_lis_free(self->lis);
			if (fork() == 0) {
				russ_svr_handler(self, sconn);

				/* failsafe exit info (if not provided) */
				russ_sconn_fatal(sconn, RUSS_MSG_NO_EXIT, RUSS_EXIT_SYS_FAILURE);
				sconn = russ_sconn_free(sconn);
				exit(0);
			}
			exit(0);
		}
		russ_sconn_close(sconn);
		sconn = russ_sconn_free(sconn);
		wpid = waitpid(pid, &wst, 0);
	}
}

/**
* Dispatches to specific server loop by server type.
*
* @param self		server object
*/
void
russ_svr_loop(struct russ_svr *self) {
	if (self == NULL) {
		return;
	}

	if (self->type == RUSS_SVR_TYPE_FORK) {
		russ_svr_loop_fork(self);
	} else if (self->type == RUSS_SVR_TYPE_THREAD) {
		fprintf(stderr, "error: use threaded libruss\n");
		exit(1);
	}
}
