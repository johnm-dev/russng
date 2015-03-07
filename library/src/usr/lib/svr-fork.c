/*
* lib/svr-fork.c
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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Server loop for forking servers.
*
* @param self		server object
*/
void
russ_svr_loop_fork(struct russ_svr *self) {
	struct russ_sconn	*sconn;
	pid_t			pid, wpid;
	int			wst;

	if (self == NULL) {
		return;
	}

	while (1) {
		if ((sconn = self->accepthandler(russ_to_deadline(self->accepttimeout), self->lisd)) == NULL) {
			fprintf(stderr, "error: cannot accept connection\n");
			sleep(1);
			continue;
		}
		if ((pid = fork()) == 0) {
			setsid();
			signal(SIGHUP, SIG_IGN);

			russ_fds_close(&self->lisd, 1);
			if (fork() == 0) {
				russ_svr_handler(self, sconn);

				/* failsafe exit info (if not provided) */
				russ_sconn_fatal(sconn, RUSS_MSG_NOEXIT, RUSS_EXIT_SYSFAILURE);
				sconn = russ_sconn_free(sconn);
				exit(0);
			}
			exit(0);
		}
		russ_sconn_close(sconn);
		sconn = russ_sconn_free(sconn);
		wpid = waitpid(pid, &wst, 0);
	}
}

/**
* Dummy function for non-threaded russ library.
*
* @param self		server object
*/
void
russ_svr_loop_thread(struct russ_svr *self) {
	fprintf(stderr, "error: use threaded libruss\n");
	exit(1);
}
