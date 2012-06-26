/*
* rusrv_relay_backend.c
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "russ_priv.h"

int
main(int argc, char **argv) {
	struct russ_conn	*conn;
	struct russ_forwarder	fwds[3];
	char			*op, *addr;
	char			**attrs, **args;
	int			size, cnt;
	char			buf[16384], *bp;
	FILE			*f;
	int			exit_status;

	/* msg size */
	bp = buf;
	if ((read(0, bp, 4) < 0)
		|| ((bp = russ_dec_I(bp, &size)) == NULL)
		|| (size > 16384)
		|| (read(0, bp, size) < 0)) {
		exit(-1);
	}
	/* op, addr */
	if (((bp = russ_dec_s(bp, &op)) == NULL)
		|| ((bp = russ_dec_s(bp, &addr)) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &attrs, &cnt)) == NULL)
		|| ((bp = russ_dec_sarray0(bp, &args, &cnt)) == NULL)) {
		exit(-1);
	}
	if ((conn = russ_dialv(-1, op, addr, attrs, args)) == NULL) {
		exit(-1);
	}

	/* start forwarder threads */
#if 0
	russ_forwarder_init(&(fwds[0]), 0, STDIN_FILENO, conn->fds[0], -1, 16384, 0);
	russ_forwarder_init(&(fwds[1]), 1, conn->fds[1], STDOUT_FILENO, -1, 16384, 0);
	russ_forwarder_init(&(fwds[2]), 1, conn->fds[2], STDERR_FILENO, -1, 16384, 0);
	if (russ_run_forwarders(3, fwds) < 0) {
		fprintf(stderr, "error: could not forward bytes\n");
		exit(-1);
	}
#endif
	russ_forwarder_init(&(fwds[0]), STDIN_FILENO, conn->fds[0], -1, 16384, 0);
	russ_forwarder_init(&(fwds[1]), conn->fds[1], STDOUT_FILENO, -1, 16384, 0);
	russ_forwarder_init(&(fwds[2]), conn->fds[2], STDERR_FILENO, -1, 16384, 0);
	if (russ_run_forwarders(RUSS_CONN_NFDS, fwds) < 0) {
		fprintf(stderr, "error: could not forward bytes\n");
		exit(-1);
	}
	russ_conn_wait(conn, &exit_status, NULL, -1);
	russ_forwarder_join(&(fwds[1]));

	russ_conn_close(conn);
	conn = russ_conn_free(conn);
	exit(0);
}