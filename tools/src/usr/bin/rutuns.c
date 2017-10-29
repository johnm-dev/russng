/*
* rutuns.c
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

#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <russ/russ.h>

#define BUFSIZE		(1<<15)
#define BUFSIZE_MAX	(1<<20)

void
print_usage(char *prog_name) {
	printf(
"usage: rutuns [<option>]\n"
"\n"
"Dial tunnel server. Receives dial request over stdin to establish a\n"
"connection.\n"
"\n"
"A successful dial will effectively connect the stdin, stdout, and\n"
"stderr of the service. Once connected, rudial forwards the stdin,\n"
"stdout, and sterr I/O data between the caller and the service.\n"
"\n"
"An exit value of < 0 indicates a failure to connect. Otherwise a 0\n"
"exit value is returned.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_cconn	*cconn = NULL;
	struct russ_sconn	*sconn = NULL;
	struct russ_req		*req = NULL;
	russ_deadline		deadline;
	char			*prog_name = NULL;
	int			bufsize, exitst;
	int			cbfd;

	signal(SIGPIPE, SIG_IGN);

	prog_name = basename(strdup(argv[0]));

	/* initialize */
	bufsize = BUFSIZE;
	cbfd = -1;
	deadline = RUSS_DEADLINE_NEVER;

	/* receive sconn */
	if ((sconn = russ_sconn_new()) == NULL) {
		exit(1);
	}
	sconn->creds.uid = getuid();
	sconn->creds.gid = getgid();
	sconn->sd = 0; /* stdin */
	deadline = russ_to_deadline(30000);
	if ((req =  russ_sconn_await_req(sconn, deadline)) == NULL) {
		exit(1);
	}

	/* dial client */
	exitst = 0;
	deadline = russ_to_deadline(30000);
	if ((cconn = russ_dialv(deadline, req->op, req->spath, req->attrv, req->argv)) == NULL) {
		fprintf(stderr, "%s\n", RUSS_MSG_NODIAL);
		exit(RUSS_EXIT_CALLFAILURE);
	}
	sconn = russ_sconn_free(sconn);

	{
		struct russ_relay		*relay = NULL;
		russ_relaystream_callback	cb = NULL;

		relay = russ_relay_new(3);
		russ_relay_addwithcallback(relay, STDIN_FILENO, cconn->fds[0], bufsize, 1, cb, (void *)((intptr_t)cbfd<<16|0));
		russ_relay_addwithcallback(relay, cconn->fds[1], STDOUT_FILENO, bufsize, 0, cb, (void *)((intptr_t)cbfd<<16|1));
		russ_relay_addwithcallback(relay, cconn->fds[2], STDERR_FILENO, bufsize, 0, cb, (void *)((intptr_t)cbfd<<16|2));

		cconn->fds[0] = -1;
		cconn->fds[1] = -1;
		cconn->fds[2] = -1;
		russ_relay_serve(relay, -1, cconn->sysfds[RUSS_CONN_SYSFD_EXIT]);
		if (russ_cconn_wait(cconn, RUSS_DEADLINE_NEVER, &exitst) < 0) {
			fprintf(stderr, "%s\n", RUSS_MSG_BADCONNEVENT);
			exitst = RUSS_EXIT_SYSFAILURE;
		}
		if (cbfd >= 0) {
			close(cbfd);
		}
	}

	russ_cconn_close(cconn);
	cconn = russ_cconn_free(cconn);

	exit(exitst);
}
