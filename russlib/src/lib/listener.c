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
* @param path	socket path
* @param mode	file mode of path
* @param uid	owner of path
* @param gid	group owner of path
* @return	listener object
*/
struct russ_listener *
russ_announce(char *path, mode_t mode, uid_t uid, gid_t gid) {
	struct russ_listener	*lis;
	struct sockaddr_un	servaddr;

	if ((lis = malloc(sizeof(struct russ_listener))) == NULL) {
		return NULL;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, path);
	if ((lis->sd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		goto free_lis;
	}
	if (((unlink(path) < 0) && (errno != ENOENT))
		|| (bind(lis->sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		|| (chmod(path, mode) < 0)
		|| (chown(path, uid, gid) < 0)
		|| (listen(lis->sd, 5) < 0)) {
		goto close_sd;
	}

	return lis;

close_sd:
	close(lis->sd);
	lis->sd = -1;
free_lis:
	free(lis);
	return NULL;
}

/**
* Answer dial.
*
* @param lis	listener object
* @param timeout	time allowed to complete operation
* @return	connection object with credentials; not fully established
*/
struct russ_conn *
russ_listener_answer(struct russ_listener *self, russ_timeout timeout) {
	struct russ_conn	*conn;
	struct sockaddr_un	servaddr;
	int			servaddr_len;
	struct pollfd		poll_fds[1];
	russ_timeout		deadline;

	if ((conn = russ_conn_new()) == NULL) {
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
	russ_close_fds(1, &conn->sd);
free_conn:
	free(conn);
	return NULL;
}

/**
* Close listener.
*
* @param self	listener object
*/
void
russ_listener_close(struct russ_listener *self) {
	if (self->sd > -1) {
		close(self->sd);
		self->sd = -1;
	}
}

/**
* Free listener object.
*
* @param self	listener object
* @return	NULL value
*/
struct russ_listener *
russ_listener_free(struct russ_listener *self) {
	free(self);
	return NULL;
}

/**
* Loop to listen for and accept incoming connections.
*
* @param self	listener object
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
