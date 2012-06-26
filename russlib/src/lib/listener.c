/*
** lib/listener.c
*/

/*
# license--start
#
# This file is part of the RUSS library.
# Copyright (C) 2012 John Marshall
#
# The RUSS library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end
*/

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Announce service as a socket file.
*
* @param saddr		socket address
* @param mode		file mode of path
* @param uid		owner of path
* @param gid		group owner of path
* @return		listener object; NULL on failure
*/
struct russ_listener *
russ_announce(char *saddr, mode_t mode, uid_t uid, gid_t gid) {
	struct russ_listener	*lis;
	struct sockaddr_un	servaddr;

	if ((saddr = russ_resolve_addr(saddr)) == NULL) {
		return NULL;
	}
	if ((lis = malloc(sizeof(struct russ_listener))) == NULL) {
		goto free_saddr;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, saddr);
	if ((lis->sd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		goto free_lis;
	}
	if (((unlink(saddr) < 0) && (errno != ENOENT))
		|| (bind(lis->sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		|| (chmod(saddr, mode) < 0)
		|| (chown(saddr, uid, gid) < 0)
		|| (listen(lis->sd, 5) < 0)) {
		goto close_sd;
	}

	free(saddr);
	return lis;

close_sd:
	close(lis->sd);
	lis->sd = -1;
free_lis:
	free(lis);
free_saddr:
	free(saddr);
	return NULL;
}

/**
* Answer dial.
*
* @param lis		listener object
* @param timeout	time allowed to complete operation
* @return		connection object with credentials (not fully established); NULL on failure
*/
struct russ_conn *
russ_listener_answer(struct russ_listener *self, russ_timeout timeout) {
	struct russ_conn	*conn;
	struct sockaddr_un	servaddr;
	int			servaddr_len;
	struct pollfd		poll_fds[1];
	russ_timeout		deadline;

	if ((self->sd < 0)
		|| ((conn = russ_conn_new()) == NULL)) {
		return NULL;
	}

	poll_fds[0].fd = self->sd;
	poll_fds[0].events = POLLIN;
	if ((timeout == RUSS_TIMEOUT_NEVER) || (timeout == RUSS_TIMEOUT_NOW)) {
		deadline = timeout;
	} else {
		deadline = (time(NULL)*1000)+timeout;
	}

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
	if (russ_get_credentials(conn->sd, &(conn->cred)) < 0) {
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
russ_listener_close(struct russ_listener *self) {
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
struct russ_listener *
russ_listener_free(struct russ_listener *self) {
	free(self);
	return NULL;
}

/**
* Loop to accept connections and call a handler in child process.
*
* Listen on the socket and fork a new process for each connection.
* All connections are answered in the main process, but requests
* are waited on and accepted in the child process. This prevents
* a DoS by a partial request from occupying the loop. Each valid
* request is the passed to the given handler. The handler is
* responsible for servicing requests, closing descriptors as
* appropriate and exiting (with russ_conn_exit()).
*
* @param self		listener object
* @param handler	handler function to call on connection
*/
void
russ_listener_loop(struct russ_listener *self, russ_req_handler handler) {
	struct russ_conn	*conn;

	while (1) {
		if ((conn = russ_listener_answer(self, RUSS_TIMEOUT_NEVER)) == NULL) {
			fprintf(stderr, "error: cannot answer connection\n");
			continue;
		}
		if (fork() == 0) {
			russ_listener_close(self);
			self = russ_listener_free(self);
			if ((russ_conn_await_request(conn) < 0)
				|| (russ_conn_accept(conn, NULL, NULL) < 0)) {
				exit(-1);
			}
			exit(handler(conn));
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
russ_listener_loop_thread(struct russ_listener *self, russ_req_handler handler) {
	pthread_attr_t			attr;
	struct russ_conn		*conn;
	struct pre_handler_thread_args	*phta;

	while (1) {
		if ((conn = russ_listener_answer(self, RUSS_TIMEOUT_NEVER)) == NULL) {
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
