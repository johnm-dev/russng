/*
** lib/handlers.c
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

#include <stdio.h>

#include "russ_priv.h"

/**
* Standard (default) accept handler which sets up standard fds and
* accepts connection.
*
* @param self		answered connection object
* @return		0 on success; -1 on failure
*/
int
russ_standard_accept_handler(struct russ_conn *self) {
	int	cfds[RUSS_CONN_NFDS], sfds[RUSS_CONN_NFDS];
	int	tmpfd;

	russ_fds_init(cfds, RUSS_CONN_NFDS, -1);
	russ_fds_init(sfds, RUSS_CONN_NFDS, -1);
	if (russ_make_pipes(RUSS_CONN_STD_NFDS, cfds, sfds) < 0) {
		fprintf(stderr, "error: cannot create pipes\n");
		return -1;
	}
	/* swap fds for stdin */
	tmpfd = cfds[0];
	cfds[0] = sfds[0];
	sfds[0] = tmpfd;

	if (russ_conn_accept(self, RUSS_CONN_STD_NFDS, cfds, sfds) < 0) {
		russ_fds_close(cfds, RUSS_CONN_STD_NFDS);
		russ_fds_close(sfds, RUSS_CONN_STD_NFDS);
		return -1;
	}
	return 0;
}

/**
* Standard (default) answer handler which answers on listener
* socket and returns a new connection object.
*
* @param self		listener object
* @param deadline	deadline to complete operation
* @return		new connection object; NULL on failure
*/
struct russ_conn *
russ_standard_answer_handler(struct russ_lis *self, russ_deadline deadline) {
	return russ_lis_answer(self, deadline);
}