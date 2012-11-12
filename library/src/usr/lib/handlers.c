/*
** lib/handlers.c
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