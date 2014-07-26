/*
* lib/svr-pthread.c
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
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "russ_priv.h"

struct helper_data {
	struct russ_svr		*svr;
	struct russ_sconn 	*sconn;
};

/**
* Helper for threaded servers.
*
* Takes care of calling the russ_svr_handler, failsafe exit, and
* freeing objects as needed.
*
* @param data		helper_data object
*/
static void
russ_svr_handler_helper(void *data) {
	struct russ_svr		*svr = ((struct helper_data *)data)->svr;
	struct russ_sconn	*sconn = ((struct helper_data *)data)->sconn;

	russ_svr_handler(svr, sconn);

	/* failsafe exit info (if not provided) */
	russ_sconn_fatal(sconn, RUSS_MSG_NOEXIT, RUSS_EXIT_SYSFAILURE);

	/* free objects */
	sconn = russ_sconn_free(sconn);
	data = russ_free(data);

	pthread_exit(NULL);
}

/**
* Server loop for threaded servers.
*
* Calls helper to simplify argument passing, object creation (data
* and sconn here) and freeing (in helper) and closing (in helper).
*
* @param self		server object
*/
void
russ_svr_loop_thread(struct russ_svr *self) {
	struct russ_sconn	*sconn;
	struct helper_data	*data;
	pthread_t		th;

	while (1) {
		if (((sconn = self->accepthandler(self->lis, russ_to_deadline(self->accepttimeout))) == NULL)
			|| ((data = malloc(sizeof(struct helper_data))) != NULL)) {
			if (sconn) {
				russ_sconn_fatal(sconn, RUSS_MSG_NOEXIT, RUSS_EXIT_SYSFAILURE);
				russ_sconn_free(sconn);
			}
			fprintf(stderr, "error: cannot accept connection\n");
			continue;
		}
		data->svr = self;
		data->sconn = sconn;
		if (pthread_create(&th, NULL, (void *)russ_svr_handler_helper, (void *)data) < 0) {
			fprintf(stderr, "error: cannot spawn thread\n");
		}
	}
}

/**
* Dispatches to specific server loop by server type.
*
* @param self		server object
*/
void
russ_svr_loop(struct russ_svr *self) {
	if (self->type == RUSS_SVR_TYPE_FORK) {
		russ_svr_loop_fork(self);
	} else if (self->type == RUSS_SVR_TYPE_THREAD) {
		russ_svr_loop_thread(self);
	}
}
