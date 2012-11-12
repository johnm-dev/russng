/*
** lib/listener.c
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
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Announce service as a socket file.
*
* If the address already exists (EADDRINUSE), then we check to see
* if anything is actually using it. If not, we remove it and try to
* set it up. If the address cannot be "bind"ed, then we exit with
* NULL.
*
* The only way to claim an address that is in use it to forcibly
* remove it from the filesystem first (unlink), then call here.
*
* @param saddr		socket address
* @param mode		file mode of path
* @param uid		owner of path
* @param gid		group owner of path
* @return		listener object; NULL on failure
*/
struct russ_lis *
russ_announce(char *saddr, mode_t mode, uid_t uid, gid_t gid) {
	struct russ_lis		*lis;
	struct sockaddr_un	servaddr;
	int			sd;

	if ((saddr = russ_resolve_addr(saddr)) == NULL) {
		return NULL;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, saddr);
	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		goto free_saddr;
	}
	if (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		if ((errno == EADDRINUSE)
			&& (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)) {
			/* is something listening? */
			if (errno != ECONNREFUSED) {
				goto close_sd;
			} else if ((unlink(saddr) < 0)
				|| (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)) {
				goto close_sd;
			}
		} else {
			goto close_sd;
		}
	}
	if ((chmod(saddr, mode) < 0)
		|| (chown(saddr, uid, gid) < 0)
		|| (listen(sd, 5) < 0)
		|| ((lis = malloc(sizeof(struct russ_lis))) == NULL)) {
		goto close_sd;
	}

	lis->sd = sd;
	free(saddr);
	return lis;

close_sd:
	close(sd);
free_saddr:
	free(saddr);
	return NULL;
}

/**
* Answer dial.
*
* @param self		listener object
* @param deadline	deadline to complete operation
* @return		connection object with credentials (not fully established); NULL on failure
*/
struct russ_conn *
russ_lis_answer(struct russ_lis *self, russ_deadline deadline) {
	struct russ_conn	*conn;
	struct sockaddr_un	servaddr;
	socklen_t		servaddr_len;
	struct pollfd		poll_fds[1];

	if ((self->sd < 0)
		|| ((conn = russ_conn_new()) == NULL)) {
		return NULL;
	}

	poll_fds[0].fd = self->sd;
	poll_fds[0].events = POLLIN;
	servaddr_len = sizeof(struct sockaddr_un);
	while (1) {
		if (russ_poll(poll_fds, 1, deadline) < 0) {
			goto free_conn;
		}
		if ((conn->sd = accept(self->sd, (struct sockaddr *)&servaddr, &servaddr_len)) >= 0) {
			break;
		}
		if (errno != EINTR) {
			goto free_conn;
		} else {
			break;
		}
	}
	if (russ_get_creds(conn->sd, &(conn->creds)) < 0) {
		goto close_sd;
	}
	return conn;

close_sd:
	russ_fds_close(&conn->sd, 1);
free_conn:
	free(conn);
	return NULL;
}

/**
* Close listener.
*
* @param self		listener object
*/
void
russ_lis_close(struct russ_lis *self) {
	if (self->sd > -1) {
		russ_fds_close(&self->sd, 1);
	}
}

/**
* Free listener object.
*
* @param self		listener object
* @return		NULL value
*/
struct russ_lis *
russ_lis_free(struct russ_lis *self) {
	free(self);
	return NULL;
}

/**
* Loop to answer and accept connections and them in a child process.
*
* The loop consists of 3 phases:
* 1) answer incoming connection on listener socket (prior to fork)
* 2) accept request on new connection object (after fork)
* 3) service request
*
* A handler for each phase can be given. However, only the request
* handler is needed. There are standard/default handlers available
* for the answer_handler, the accept_handler when each are set to
* NULL.
*
* The request handler is responsible for servicing requests, closing
* descriptors as appropriate and exiting (with russ_conn_exit()).
*
* @param self		listener object
* @param answer_handler	handler function to call on listener object
* @param accept_handler	handler function to call new connection
*			object (from answer)
* @param req_handler	handler function to call on accepted
*/
void
russ_lis_loop(struct russ_lis *self, russ_answer_handler answer_handler,
	russ_accept_handler accept_handler, russ_req_handler req_handler) {

	struct russ_conn	*conn;

	if (answer_handler == NULL) {
		answer_handler = russ_standard_answer_handler;
	}
	if (accept_handler == NULL) {
		accept_handler = russ_standard_accept_handler;
	}

	while (1) {
		if ((conn = answer_handler(self, RUSS_DEADLINE_NEVER)) == NULL) {
			fprintf(stderr, "error: cannot answer connection\n");
			continue;
		}
		if (fork() == 0) {
			setsid();
			russ_lis_close(self);
			self = russ_lis_free(self);
			if ((russ_conn_await_request(conn, RUSS_DEADLINE_NEVER) < 0)
				|| (accept_handler(conn) < 0)) {
				exit(-1);
			}
			req_handler(conn);
			russ_conn_fatal(conn, RUSS_MSG_NO_EXIT, RUSS_EXIT_SYS_FAILURE);
			russ_conn_close(conn);
			conn = russ_conn_free(conn);
			exit(0);
		}
		russ_conn_close(conn);
		conn = russ_conn_free(conn);
	}
}

#if 0
struct pre_handler_thread_args {
	struct russ_conn	*conn;
	russ_req_handler	handler;
};

/**
* Special function to manage conn after listener answer and call
* handler.
*
* @param vp	pointer to pre_handler_thread_args object
*/
static void
__pre_handler_thread(void *vp) {
	struct pre_handler_thread_args	*phta;
	struct russ_conn		*conn;
	russ_req_handler		handler;
	int				exit_value;

	/* unpack */
	phta = (struct pre_handler_thread_args *)vp;
	conn = phta->conn;
	handler = phta->handler;
	free(phta);

	/* await/accept and call handler */
	if ((russ_conn_await_request(conn) < 0)
		|| (russ_conn_accept(conn, NULL, NULL) < 0)) {
		pthread_exit(-1);
	}
	exit_value = handler(conn);

	/* clean up and exit */
	russ_conn_close(conn);
	conn = russ_conn_free(conn);
	pthread_exit(exit_value);
}

/**
* Loop to listen for and accept incoming connections spawning a
* thread for each connection.
*
* @param self		listener object
* @param handler	handler function to call on connection
*/
void
russ_lis_loop_thread(struct russ_lis *self, russ_req_handler handler) {
	pthread_attr_t			attr;
	struct russ_conn		*conn;
	struct pre_handler_thread_args	*phta;

	while (1) {
		if ((conn = russ_lis_answer(self, RUSS_TIMEOUT_NEVER)) == NULL) {
			fprintf(stderr, "error: cannot answer connection\n");
			continue;
		}
		pthread_attr_init(&attr);
		/* what should the stack size be? */
		pthread_attr_setstacksize(&attr, (1<<20)*2);
		if (pthread_create(NULL, &attr, __pre_handler_thread, (void *)) < 0) {
			return -1;
		}
		pthread_attr_destroy(&attr);
	}
}
#endif
