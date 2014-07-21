/*
* rusrv_srelay_backend.c
*/

/*
# license--start
#
# This file is part of RUSS tools.
# Copyright (C) 2012 John Marshall
#
# RUSS tools is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end
*/

#include <memory.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "disp.h"
#include "russ_priv.h"

/**
* Set up socket to listen.
*
* @param hostname	remote host
* @param port		remote port
* @return		listen socket descriptor; -1 on failure
*/
int
setup_socket(char *hostname, int port) {
	struct sockaddr_in	servaddr;
	struct hostent		*hent;
	int			sd = -1;

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
#if 0
	if ((hent = gethostbyname(hostname)) == NULL) {
		return -1;
	}
	servaddr.sin_addr.s_addr = *((unsigned long *)hent->h_addr_list[0]);
#endif
	if (((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		|| (bind(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		|| (listen(sd, 30) < 0)) {
		if (sd >= 0) {
			close(sd);
		}
		return -1;
	}
	return sd;
}

/**
* Forward bytes between server and client
*
* @param conn		connection object
* @param sd		socket descriptor
* @return		0 on success; -1 on failure (of setup)
*/
int
forward_bytes(struct russ_conn *conn, int sd) {
	struct dispatcher	*disp;
	struct rw		*rw;
	int			i, ev;

	if ((disp = dispatcher_new(4, sd)) == NULL) {
		return -1;
	}

	if ((dispatcher_add_rw(disp, rw_new(DISPATCHER_WRITER, conn->fds[0])) < 0)
		|| (dispatcher_add_rw(disp, rw_new(DISPATCHER_READER, conn->fds[1])) < 0)
		|| (dispatcher_add_rw(disp, rw_new(DISPATCHER_READER, conn->fds[2])) < 0)
		|| (dispatcher_add_rw(disp, rw_new(DISPATCHER_READER, conn->sysfds[RUSS_CONN_SYSFD_EXIT])) < 0)) {
		ev = -1;
		goto cleanup;
	}
	dispatcher_loop(disp);
	ev = 0;
cleanup:
	for (i = 0; i < disp->nrws; i++) {
		disp->rws[i] = rw_destroy(disp->rws[i]);
	}
	dispatcher_destroy(disp);
	return ev;
}

/**
* Decode incoming dial info and redial.
*
* @param sd		socket descriptor to srelay
* @return		connection object; NULL on failure
*/
struct russ_conn *
redial(int sd) {
	struct russ_conn	*conn;
	char			*op;
	char			*addr;
	char			**attrs, **args;
	char			buf[16384], *bp;
	int			size, cnt;

	/* msg size */
	bp = buf;
	if ((read(sd, bp, 4) < 0)
		|| ((bp = russ_dec_I(bp, &size)) == NULL)
		|| (size > 16384)
		|| (read(sd, bp, size) < 0)) {
		return NULL;
	}

	/* op, addr */
	if (((bp = russ_dec_s(bp, &op)) == NULL)
		|| ((bp = russ_dec_s(bp, &addr)) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &attrs, &cnt)) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &args, &cnt)) == NULL)) {
		return NULL;
	}

//fprintf(stderr, "op (%s) addr (%s)\n", op, addr);
	if ((conn = russ_dialv(RUSS_DEADLINE_NEVER, op, addr, attrs, args)) == NULL) {
		return NULL;
	}
//fprintf(stderr, "conn (%p)\n", conn);
	return conn;
}

/**
* Connect to remote server.
*
* @param hostname	remote host
* @param port		remote port
* @return		socket descriptor
*/
int
main(int argc, char **argv) {
	struct sockaddr_in	cliaddr;
	socklen_t		clilen;
	struct russ_conn	*conn;
	int			lsd, csd;
	int			size, cnt;

	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	if ((lsd = setup_socket("", 8001)) < 0) {
		exit(1);
	}
	while (1) {
		if ((csd = accept(lsd, (struct sockaddr *)&cliaddr, &clilen)) < 0) {
			continue;
		}
		if (fork() == 0) {
			close(lsd);
			//close(0);
			//close(1);
			//close(2);
fprintf(stderr, "REDIALING\n");
			if ((conn = redial(csd)) == NULL) {
fprintf(stderr, "REDIALING FAILED\n");
				exit(0);
			}
fprintf(stderr, "FORWARDING\n");
			forward_bytes(conn, csd);
			exit(0);
		}
		close(csd);
	}
}
