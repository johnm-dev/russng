/*
* lib/svr.c
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

#include "russ/priv.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX	64
#endif /* HOST_NAME_MAX */

/**
* Create russ_svr object.
*
* @param root		root service node object
* @param type		server type (see RUSS_SVR_TYPE_*)
* @param lisd		initial listening socket descriptor
* @return		russ_svr object; NULL on failure
*/
struct russ_svr *
russ_svr_new(struct russ_svcnode *root, int type, int lisd) {
	struct russ_svr	*self = NULL;

	if ((self = russ_malloc(sizeof(struct russ_svr))) == NULL) {
		return NULL;
	}
	self->root = root;
	self->type = type;
	self->mpid = getpid();
	self->ctime = russ_gettime();
	self->saddr = NULL;
	self->lisd = lisd;
	self->closeonaccept = 0;
	self->accepthandler = russ_sconn_accepthandler;
	self->accepttimeout = RUSS_SVR_TIMEOUT_ACCEPT;
	self->allowrootuser = 1;
	self->answerhandler = russ_sconn_answerhandler;
	self->awaittimeout = RUSS_SVR_TIMEOUT_AWAIT;
	self->autoswitchuser = 1;
	self->matchclientuser = 0;
	self->help = NULL;

	return self;
}

/**
* Free russ_svr object.
*
* @param		russ_svr object
* @return		NULL
*/
struct russ_svr *
russ_svr_free(struct russ_svr *self) {
	if (self) {
		/* own copy */
		/* TODO: only the root node is freed */
		self->root = russ_svcnode_free(self->root);
		self->saddr = russ_free(self->saddr);
		self->help = russ_free(self->help);
	}
	return NULL;
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
	return self->accepthandler(deadline, self->lisd);
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
	if ((self == NULL)
		|| (handler == NULL)) {
		return -1;
	}
	self->accepthandler = handler;
	return 0;
}

/**
* Set timeout for accept call.
*
* @param self		russ server object
* @param value		timeout in milliseconds
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_accepttimeout(struct russ_svr *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->accepttimeout = value;
	return 0;
}

/**
* Set flag for check that allows root user.
*
* If enabled, the server handler will verify and allow that the
* getuid() is root/0. If not enabled, root is disallowed!
*
* @param self		russ server object
* @param value		0 to disable; 1 to enable
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_allowrootuser(struct russ_svr *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->allowrootuser = value;
	return 0;
}

/**
* Register an alternative answer handler.
*
* @param self		russ server object
* @param handler	answer handler
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_answerhandler(struct russ_svr *self, russ_answerhandler handler) {
	if ((self == NULL)
		|| (handler == NULL)) {
		return -1;
	}
	self->answerhandler = handler;
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
russ_svr_set_autoswitchuser(struct russ_svr *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->autoswitchuser = value;
	return 0;
}

/**
* Set to close listen socket after 1 successful accept.
*
* @param self		russ server object
* @param value		0 to disable; 1 to enable
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_closeonaccept(struct russ_svr *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->closeonaccept = value;
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
russ_svr_set_help(struct russ_svr *self, const char *help) {
	if ((self == NULL)
		|| ((self->help = strdup(help)) == NULL)) {
		return -1;
	}
	return 0;
}

/**
* Set flag for check that getuid() matches client.
*
* If enabled, the server handler will verify that the getuid()
* matches the client use.
*
* @param self		russ server object
* @param value		0 to disable; 1 to enable
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_matchclientuser(struct russ_svr *self, int value) {
	if (self == NULL) {
		return -1;
	}
	self->matchclientuser = value;
	return 0;
}

/**
* Set the service tree root node.
*
* @param self		server object
* @param root		root service node
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_root(struct russ_svr *self, struct russ_svcnode *root) {
	self->root = root;
	return 0;
}

/**
* Set the socket descriptor for receiving connections.
*
* @param self		server object
* @param lisd		listening socket descriptor
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_lisd(struct russ_svr *self, int lisd) {
	self->lisd = lisd;
	return 0;
}

/**
* Set server type.
*
* @param self		server object
* @param type		server type (e.g., RUSS_SVR_TYPE_FORK)
* @return		0 on success; -1 on failure
*/
int
russ_svr_set_type(struct russ_svr *self, int type) {
	self->type = type;
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
	struct russ_sess	*sess = NULL;
	struct russ_req		*req = NULL;
	struct russ_svcnode	*node = NULL;
	char			mpath[RUSS_REQ_SPATH_MAX] = "";

	if (self == NULL) {
		return;
	}

	if ((req = russ_sconn_await_req(sconn, russ_to_deadline(self->awaittimeout))) == NULL) {
		/* failure */
		goto cleanup;
	}

	/* validate opnum */
	if (req->opnum == RUSS_OPNUM_NOTSET) {
		/* invalid opnum */
		russ_sconn_fatal(sconn, RUSS_MSG_BADOP, RUSS_EXIT_SYSFAILURE);
		goto cleanup;
	}
	/* validate spath: must be absolute or empty */
	if ((req->spath[0] != '/') && (req->spath[0] != '\0')) {
		/* invalid spath */
		goto cleanup;
	}

	if ((node = russ_svcnode_find(self->root, req->spath, mpath, sizeof(mpath))) == NULL) {
		/* we need standard fds */
		russ_sconn_answerhandler(sconn);
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
		goto cleanup;
	}

	if ((node->autoanswer)
		&& ((self->answerhandler == NULL) || (self->answerhandler(sconn) < 0))) {
		goto cleanup;
	}

	/* auto switch user if requested */
	if (self->autoswitchuser) {
		if ((chdir("/") < 0)
			|| (russ_env_clear() < 0)
			|| (russ_switch_userinitgroups(sconn->creds.uid, sconn->creds.gid) < 0)
			|| (russ_env_setdefaults() < 0)) {
			russ_sconn_fatal(sconn, RUSS_MSG_NOSWITCHUSER, RUSS_EXIT_FAILURE);
			goto cleanup;
		}
	}

	/* verify/enforce proper user */
	if (self->matchclientuser) {
		if (getuid() != sconn->creds.uid) {
			russ_sconn_fatal(sconn, RUSS_MSG_BADUSER, RUSS_EXIT_FAILURE);
			goto cleanup;
		}
	}

	/* verify/enforce root user */
	if (!self->allowrootuser) {
		if (getuid() == 0) {
			russ_sconn_fatal(sconn, RUSS_MSG_BADUSER, RUSS_EXIT_FAILURE);
			goto cleanup;
		}
	}

	/* prepare session object */
	sess = russ_sess_new(self, sconn, req, mpath);

	/* call handler, if available */
	if (node) {
		if (node->handler) {
			node->handler(sess);
		}
	} else {
		russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
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
				/* return list if _not_ wildcard initial child */
				if (!node->children->wildcard) {
					for (node = node->children; node != NULL; node = node->next) {
						russ_dprintf(sconn->fds[1], "%s\n", node->name);
					}
				}
				russ_sconn_exit(sconn, RUSS_EXIT_SUCCESS);
			} else if (node->wildcard) {
				russ_sconn_fatal(sconn, RUSS_MSG_NOLIST, RUSS_EXIT_SUCCESS);
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
		if (sconn->creds.uid == getuid()) {
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
		russ_sconn_fatal(sconn, RUSS_MSG_BADOP, RUSS_EXIT_FAILURE);
	}

cleanup:
	/* clean up: on error and fallthrough on success */
	if (sess) {
		sess = russ_sess_free(sess);
	}
	if (req != NULL) {
		req = russ_req_free(req);
	}
	russ_sconn_fatal(sconn, RUSS_MSG_NOSERVICE, RUSS_EXIT_FAILURE);
	russ_sconn_close(sconn);
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
		russ_svr_loop_thread(self);
	}
}
