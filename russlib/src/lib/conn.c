/*
** lib/conn.c
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
* Initialize descriptor array to value.
*
* @param count	size of fds array
* @param fds	descriptor array
* @param value	initialization value
*/
static void
__init_fds(int count, int *fds, int value) {
	int	i;

	for (i = 0; i < count; i++) {
		fds[i] = value;
	}
}

/**
* Close descriptor and set array values to -1.
*
* @param count	size of fds array
* @param fds	descriptor array
*/
static void
__close_fds(int count, int *fds) {
	int	i;

	for (i = 0; i < count; i++) {
		if (fds[i] > -1) {
			close(fds[i]);
			fds[i] = -1;
		}
	}
}

/**
* Close a descriptor of the connection.
*
* @param conn	connection object
* @param index	index of the descriptor
*/
void
russ_close_fd(struct russ_conn *conn, int index) {
	__close_fds(1, &(conn->fds[index]));
}

/**
* Create and initialize a connection object.
*
* @return	a new, initialized connection object
*/
static struct russ_conn *
__new_conn(void) {
	struct russ_conn	*conn;

	if ((conn = malloc(sizeof(struct russ_conn))) == NULL) {
		return NULL;
	}
	conn->cred.pid = -1;
	conn->cred.uid = -1;
	conn->cred.gid = -1;
	if (russ_init_request(conn, NULL, NULL, NULL, NULL, NULL) < 0) {
		goto free_conn;
	}
	conn->sd = -1;
	__init_fds(3, conn->fds, -1);

	return conn;
free_conn:
	free(conn);
	return NULL;
}

/**
* Connect, send args, and receive descriptors.
*
* @param path	socket path
* @return	descriptor value; -1 on error
*/
static int
__connect(char *path) {
	struct sockaddr_un	servaddr;
	int			sd;

	if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) >= 0) {
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sun_family = AF_UNIX;
		strcpy(servaddr.sun_path, path);
		if (connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
			close(sd);
			sd = -1;
		}
	}
	return sd;
}

/**
* Get fds from connection.
*
* @param conn	connection object
* @return	0 on succes; -1 on error
*/
static int
__recvfds(struct russ_conn *conn) {
	int	i;

	for (i = 0; i < 3; i++) {
		if (russ_recvfd(conn->sd, &(conn->fds[i])) < 0) {
			return -1;
		}
	}
	return 0;
}

/**
* Send cfds over connection, close cfds, and save sfds to connection
* object.
*
* @param conn	connection object
* @param cfds	client-side descriptors
* @param sfds	server-side descriptors
* @return	0 on success; -1 on error
*/
static int
__sendfds(struct russ_conn *conn, int *cfds, int *sfds) {
	int	i;

	for (i = 0; i < 3; i++) {
		if (russ_sendfd(conn->sd, cfds[i]) < 0) {
			return -1;
		}
		close(cfds[i]);
		cfds[i] = -1;
		conn->fds[i] = sfds[i];
	}
	return 0;
}

/**
* Make pipes. A failure releases all created pipes.
*
* @param count	# of pipes to make; minimum size of rfds and wfds
* @param rfds	array to store created read fds
* @param wfds	array to store created write fds 
* @return	0 on success; -1 on error
*/
static int
__make_pipes(int count, int *rfds, int *wfds) {
	int	i, pfds[2];

	for (i = 0; i < count; i++) {
		if (pipe(pfds) < 0) {
			goto close_fds;
		}
		rfds[i] = pfds[0];
		wfds[i] = pfds[1];
	}
	return 0;

close_fds:
	__close_fds(i, rfds);
	__close_fds(i, wfds);
	return -1;
}

/* ---------------------------------------- */

/**
* Dial service.
*
* @param timeout	time allowed to complete operation
* @param saddr	service address
* @param op	operation string
* @param attrv	array of attributes (as name=value strings); NULL-terminated list
* @param argv	array of args; NULL-terminated list
* @return	connection object; NULL on failure
*/
struct russ_conn *
russ_dialv(russ_timeout timeout, char *addr, char *op, char **attrv, char **argv) {
	struct russ_conn	*conn;
	struct russ_request	*req;
	char			*path, *spath;

	/* saddr->path, spath */
	if ((path = russ_find_service_addr(saddr)) == NULL) {
		return NULL;
	}
	spath = saddr+strlen(path);

	/* steps to set up conn object */
	if ((conn = __new_conn()) == NULL) {
		goto free_path;
	}
	if (((conn->sd = __connect(targ->saddr)) < 0)
		|| (russ_init_request(conn, RUSS_PROTOCOL_STRING, targ->spath, op, attrv, argv) < 0)
		|| (russ_send_request(timeout, conn) < 0)
		|| (__recvfds(conn) < 0)) {
		goto close_conn;
	}
	__close_fds(1, &conn->sd);	/* sd not needed anymore */
	return conn;

close_conn:
	russ_close_conn(conn);
	free(conn);
free_path:
	free(path);
	return NULL;
}

/**
* Dial service using variable argument list.
*
* @param timeout	time allowed to complete operation
* @param saddr	service address
* @param op	operation string
* @param attrv	array of attributes (as name=value strings)
* @param ...	variable argument list of "char *" with NULL sentinel
* @return	connection object; NULL on failure
*/
struct russ_conn *
russ_diall(russ_timeout timeout, char *saddr, char *op, char **attrv, ...) {
	struct russ_conn	*conn;
	va_list			ap;
	void			*p;
	int			i, argc;
	char			**argv;

	/* count args */
	va_start(ap, attrv);
	for (argc = 0; argc < RUSS_MAX_ARGC; argc++) {
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

	conn = russ_dialv(timeout, addr, op, attrv, argv);
	free(argv);

	return conn;
}

/**
* Close connection.
*
* @param conn	connection object
*/
void
russ_close_conn(struct russ_conn *conn) {
	__close_fds(3, conn->fds);
	__close_fds(1, &conn->sd);
}

/**
* Free connection object.
*
* @param conn	connection object
* @return	NULL value
*/
struct russ_conn *
russ_free_conn(struct russ_conn *conn) {
	russ_free_request_members(conn);
	free(conn);
	return NULL;
}

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
* @param timeout	time allowed to complete operation
* @param lis	listener object
* @return	connection object with credentials; not fully established
*/
struct russ_conn *
russ_answer(russ_timeout timeout, struct russ_listener *lis) {
	struct russ_conn	*conn;
	struct sockaddr_un	servaddr;
	int			servaddr_len;
	struct pollfd		poll_fds[1];
	russ_timeout		deadline;

	if ((conn = __new_conn()) == NULL) {
		return NULL;
	}

	poll_fds[0].fd = lis->sd;
	poll_fds[0].events = POLLIN;
	if ((timeout == RUSS_TIMEOUT_NEVER) || (timeout == RUSS_TIMEOUT_NOW)) {
		deadline = timeout;
	} else {
		deadline = (time(NULL)*1000)+timeout;
	}

	servaddr_len = sizeof(struct sockaddr_un);
	while (1) {
		if (russ_poll(deadline, poll_fds, 1) < 0) {
			goto free_conn;
		}
		if ((conn->sd = accept(lis->sd, (struct sockaddr *)&servaddr, &servaddr_len)) >= 0) {
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
	__close_fds(1, &conn->sd);
free_conn:
	free(conn);
	return NULL;
}

/**
* Accept request. Socket is closed.
*
* @param conn	answered connection object
* @param cfds	array of descriptors to send to client
* @param sfds	array of descriptors for server side
* @return	0 on success; -1 on error
*/
int
russ_accept(struct russ_conn *conn, int *cfds, int *sfds) {
	int	_cfds[3], _sfds[3], fds[2], tmpfd;
	int	i;

	if ((cfds == NULL) && (sfds == NULL)) {
		cfds = _cfds;
		sfds = _sfds;
		__init_fds(3, cfds, 0);
		__init_fds(3, sfds, 0);
		if (__make_pipes(3, cfds, sfds) < 0) {
			fprintf(stderr, "error: cannot create pipes\n");
			return -1;
		}
		/* switch 0 elements */
		tmpfd = cfds[0];
		cfds[0] = sfds[0];
		sfds[0] = tmpfd;
	}

	if (__sendfds(conn, cfds, sfds) < 0) {
		goto close_fds;
	}
	fsync(conn->sd);
	__close_fds(1, &conn->sd);
	return 0;

close_fds:
	__close_fds(3, cfds);
	__close_fds(3, sfds);
	__close_fds(1, &conn->sd);
	return -1;
}

/**
* Close listener.
*
* @param lis	listener object
*/
void
russ_close_listener(struct russ_listener *lis) {
	if (lis->sd > -1) {
		close(lis->sd);
		lis->sd = -1;
	}
}

/**
* Free listener object.
*
* @param lis	listener object
* @return	NULL value
*/
struct russ_listener *
russ_free_listener(struct russ_listener *lis) {
	free(lis);
	return NULL;
}

