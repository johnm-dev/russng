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
	russ_close_fds(i, rfds);
	russ_close_fds(i, wfds);
	return -1;
}

/**
* Close a descriptor of the connection.
*
* @param conn	connection object
* @param index	index of the descriptor
*/
void
russ_conn_close_fd(struct russ_conn *conn, int index) {
	russ_close_fds(1, &(conn->fds[index]));
}

/**
* Helper to initialize connection request.
*/
int
russ_conn_init_request(struct russ_conn *conn, char *protocol_string, char *spath, char *op, char **attrv, char **argv) {
	return russ_request_init(&(conn->req), protocol_string, spath, op, attrv, argv);
}

/**
* Helper to free connection request members.
*/
void
russ_conn_free_request_members(struct russ_conn *conn) {
	russ_request_free_members(&(conn->req));
}

/**
* Create and initialize a connection object.
*
* @return	a new, initialized connection object
*/
struct russ_conn *
russ_conn_new(void) {
	struct russ_conn	*conn;

	if ((conn = malloc(sizeof(struct russ_conn))) == NULL) {
		return NULL;
	}
	conn->cred.pid = -1;
	conn->cred.uid = -1;
	conn->cred.gid = -1;
	if (russ_conn_init_request(conn, NULL, NULL, NULL, NULL, NULL) < 0) {
		goto free_conn;
	}
	conn->sd = -1;
	russ_init_fds(3, conn->fds, -1);

	return conn;
free_conn:
	free(conn);
	return NULL;
}

/**
* Get fds from connection.
*
* @param conn	connection object
* @return	0 on succes; -1 on error
*/
static int
russ_conn_recvfds(struct russ_conn *conn) {
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
russ_conn_sendfds(struct russ_conn *conn, int *cfds, int *sfds) {
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

/* ---------------------------------------- */

/**
* Dial service.
*
* @param timeout	time allowed to complete operation
* @param op	operation string
* @param addr	full service address
* @param attrv	array of attributes (as name=value strings); NULL-terminated list
* @param argv	array of args; NULL-terminated list
* @return	connection object; NULL on failure
*/
struct russ_conn *
russ_dialv(russ_timeout timeout, char *op, char *addr, char **attrv, char **argv) {
	struct russ_conn	*conn;
	struct russ_request	*req;
	struct russ_target	*targ;
	char			*path, *spath;

	if ((targ = russ_find_service_target(addr)) == NULL) {
		return NULL;
	}

	/* steps to set up conn object */
	if ((conn = russ_conn_new()) == NULL) {
		goto free_targ;
	}
	if (((conn->sd = __connect(targ->saddr)) < 0)
		|| (russ_conn_init_request(conn, RUSS_PROTOCOL_STRING, targ->spath, op, attrv, argv) < 0)
		|| (russ_conn_send_request(conn, timeout) < 0)
		|| (russ_conn_recvfds(conn) < 0)) {
		goto close_conn;
	}
	free(targ);
	russ_close_fds(1, &conn->sd);	/* sd not needed anymore */
	return conn;

close_conn:
	russ_conn_close(conn);
	free(conn);
free_targ:
	free(targ);
	return NULL;
}

/**
* Dial service using variable argument list.
*
* @param timeout	time allowed to complete operation
* @param op	operation string
* @param addr	full service address
* @param attrv	array of attributes (as name=value strings)
* @param ...	variable argument list of "char *" with NULL sentinel
* @return	connection object; NULL on failure
*/
struct russ_conn *
russ_diall(russ_timeout timeout, char *op, char *addr, char **attrv, ...) {
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
* Accept request. Socket is closed.
*
* @param conn	answered connection object
* @param cfds	array of descriptors to send to client
* @param sfds	array of descriptors for server side
* @return	0 on success; -1 on error
*/
int
russ_conn_accept(struct russ_conn *conn, int *cfds, int *sfds) {
	int	_cfds[3], _sfds[3], fds[2], tmpfd;
	int	i;

	if ((cfds == NULL) && (sfds == NULL)) {
		cfds = _cfds;
		sfds = _sfds;
		russ_init_fds(3, cfds, 0);
		russ_init_fds(3, sfds, 0);
		if (__make_pipes(3, cfds, sfds) < 0) {
			fprintf(stderr, "error: cannot create pipes\n");
			return -1;
		}
		/* switch 0 elements */
		tmpfd = cfds[0];
		cfds[0] = sfds[0];
		sfds[0] = tmpfd;
	}

	if (russ_conn_sendfds(conn, cfds, sfds) < 0) {
		goto close_fds;
	}
	fsync(conn->sd);
	russ_close_fds(1, &conn->sd);
	return 0;

close_fds:
	russ_close_fds(3, cfds);
	russ_close_fds(3, sfds);
	russ_close_fds(1, &conn->sd);
	return -1;
}

/*
** wait for the request to come; store in conn
*/
int
russ_conn_await_request(struct russ_conn *conn) {
	struct russ_request	*req;
	char			buf[MAX_REQUEST_BUF_SIZE], *bp;
	int			alen, size;

	/* get request size, load, and upack */
	bp = buf;
	if ((russ_readn(conn->sd, bp, 4) < 0)
		|| ((bp = russ_dec_i(bp, &size)) == NULL)
		|| (russ_readn(conn->sd, bp, size) < 0)) {
		return -1;
	}

	req = &(conn->req);
	if (((bp = russ_dec_s(bp, &(req->protocol_string))) == NULL)
		|| (strcmp(RUSS_PROTOCOL_STRING, req->protocol_string) != 0)
		|| ((bp = russ_dec_s(bp, &(req->spath))) == NULL)
		|| ((bp = russ_dec_s(bp, &(req->op))) == NULL)
		|| ((bp = russ_dec_s_array0(bp, &(req->attrv), &alen)) == NULL)
		|| ((bp = russ_dec_s_array0(bp, &(req->argv), &alen)) == NULL)) {

		goto free_req_items;
	}
	return 0;
free_req_items:
	russ_conn_free_request_members(conn);
	return -1;
}

/**
* Close connection.
*
* @param conn	connection object
*/
void
russ_conn_close(struct russ_conn *conn) {
	russ_close_fds(3, conn->fds);
	russ_close_fds(1, &conn->sd);
}

/**
* Free connection object.
*
* @param conn	connection object
* @return	NULL value
*/
struct russ_conn *
russ_conn_free(struct russ_conn *conn) {
	russ_conn_free_request_members(conn);
	free(conn);
	return NULL;
}

/**
* Send request over conn.
*
* @param conn	connection object
* @param timeout	time in which to complete the send
* @return	0 on success, -1 on error
*/
int
russ_conn_send_request(struct russ_conn *conn, russ_timeout timeout) {
	struct russ_request	*req;
	char			buf[MAX_REQUEST_BUF_SIZE], *bp, *bend;

	req = &(conn->req);
	bp = buf;
	bend = buf+sizeof(buf);
	if (((bp = russ_enc_i(bp, bend, 0)) == NULL)
		|| ((bp = russ_enc_string(bp, bend, req->protocol_string)) == NULL)
		|| ((bp = russ_enc_string(bp, bend, req->spath)) == NULL)
		|| ((bp = russ_enc_string(bp, bend, req->op)) == NULL)
		|| ((bp = russ_enc_s_array0(bp, bend, req->attrv)) == NULL)
		|| ((bp = russ_enc_s_array0(bp, bend, req->argv)) == NULL)) {
		//|| ((bp = russ_enc_s_arrayn(bp, bend, req->argv, req->argc)) == NULL)) {
		return -1;
	}

	/* patch size and send */
	russ_enc_i(buf, bend, bp-buf-4);
	if (russ_writen_timeout(conn->sd, buf, bp-buf, timeout) < bp-buf) {
		return -1;
	}
	return 0;
}
