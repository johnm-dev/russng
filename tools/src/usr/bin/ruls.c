/*
* ruls.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define USE_RUSS_FWD

#include "russ.h"

#ifdef USE_RUSS_FWD
#include "russ_fwd.h"
#endif /* USE_RUSS_FWD */

#ifdef USE_RUSS_RELAY
#include "russ_relay.h"
#endif /* USE_RUSS_RELAY */

void
print_usage(char *prog_name) {
	printf(
"usage: ruls [-t|--timeout <seconds>] <spath>\n"
"       ruls [-h|--help]\n"
"\n"
"List service(s) at <spath> (may also be a directory path).\n"
"Directory listings show service files, symlinks, and directories\n"
"only. Directories are indicated by a trailing / and the ./ entry\n"
"is always listed for a valid directory.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_conn	*conn;
	struct stat		st;
	char			*prog_name;
	char			*spath;
	russ_deadline		deadline;
	int			timeout;
	int			exit_status;

	prog_name = basename(strdup(argv[0]));

	deadline = RUSS_DEADLINE_NEVER;

	/* parse args */
	if (argc == 2) {
		if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
			print_usage(prog_name);
			exit(0);
		}
		spath = argv[1];
	} else if (argc == 4) {
		if ((strcmp(argv[1], "-t") == 0) || (strcmp(argv[1], "--timeout") == 0)) {
			if (sscanf(argv[2], "%d", (int *)&timeout) < 0) {
				fprintf(stderr, "error: bad timeout value\n");
				exit(1);
			}
			timeout *= 1000;
			deadline = russ_to_deadline(timeout);
		} else {
			fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
		}
		spath = argv[3];
	} else {
		spath = ".";
	}

	/* resolve before calling russ_list() */
	spath = russ_spath_resolve(spath);

	/* TODO: clean up exit_status usage */

	/* call russ_list() only if a socket file; otherwise list dir */
	exit_status = 0;
	if ((stat(spath, &st) < 0) || S_ISSOCK(st.st_mode)) {
		conn = russ_list(deadline, spath);

		if (conn == NULL) {
			fprintf(stderr, "%s\n", RUSS_MSG_NO_DIAL);
			exit(RUSS_EXIT_CALL_FAILURE);
		}

#ifdef USE_RUSS_FWD
		{
			struct russ_fwd		fwds[RUSS_CONN_NFDS];
			/*
			** initialize forwarders (handing off fds; but not
			** closing) and start threads
			*/
			russ_fwd_init(&(fwds[0]), 0, STDIN_FILENO, conn->fds[0], -1, 16384, 0, 1);
			russ_fwd_init(&(fwds[1]), 0, conn->fds[1], STDOUT_FILENO, -1, 16384, 0, 1);
			russ_fwd_init(&(fwds[2]), 0, conn->fds[2], STDERR_FILENO, -1, 16384, 0, 1);
			conn->fds[0] = -1;
			conn->fds[1] = -1;
			conn->fds[2] = -1;
			if (russ_fwds_run(fwds, RUSS_CONN_STD_NFDS-1) < 0) {
				fprintf(stderr, "error: could not forward bytes\n");
				exit(RUSS_EXIT_SYS_FAILURE);
			}

			/* wait for exit */
			if (russ_conn_wait(conn, &exit_status, -1) < 0) {
				fprintf(stderr, "%s\n", RUSS_MSG_BAD_CONN_EVENT);
				exit_status = RUSS_EXIT_SYS_FAILURE;
			}
			russ_fwd_join(&(fwds[1]));
		}
#endif /* USE_RUSS_FWD */

#ifdef USE_RUSS_RELAY
		{
			struct russ_relay	*relay;

			relay = russ_relay_new(3*2);
			russ_relay_add(relay, RUSS_RELAYDIR_WE, STDIN_FILENO, RELAY_BUFSIZE, 1, conn->fds[0], RELAY_BUFSIZE, 1);
			russ_relay_add(relay, RUSS_RELAYDIR_EW, STDOUT_FILENO, RELAY_BUFSIZE, 1, conn->fds[1], RELAY_BUFSIZE, 1);
			russ_relay_add(relay, RUSS_RELAYDIR_EW, STDERR_FILENO, RELAY_BUFSIZE, 0, conn->fds[2], RELAY_BUFSIZE, 1);

			conn->fds[0] = -1;
			conn->fds[1] = -1;
			conn->fds[2] = -1;
			russ_relay_serve(relay, -1);

			/* wait for exit */
			if (debug) {
				fprintf(stderr, "debug: waiting for connection exit\n");
			}
			if (russ_conn_wait(conn, &exit_status, -1) < 0) {
				fprintf(stderr, "%s\n", RUSS_MSG_BAD_CONN_EVENT);
				exit_status = RUSS_EXIT_SYS_FAILURE;
			}
		}
#endif /* USE_RUSS_RELAY */

		russ_conn_close(conn);
		conn = russ_conn_free(conn);
	} else if (S_ISDIR(st.st_mode)) {
		DIR		*dir;
		struct dirent	*dent;
		char		path[RUSS_REQ_SPATH_MAX];

		if ((dir = opendir(spath)) == NULL) {
			fprintf(stderr, "error: cannot open directory\n");
			exit_status = 1;
		} else {
			while ((dent = readdir(dir)) != NULL) {
				if (strcmp(dent->d_name, "..") == 0) {
					continue;
				}
				if ((snprintf(path, sizeof(path), "%s/%s", spath, dent->d_name) < 0)
					|| (lstat(path, &st) < 0)) {
					/* problem */
					continue;
				}
				if (S_ISDIR(st.st_mode)) {
					printf("%s/\n", dent->d_name);
				} else if (S_ISSOCK(st.st_mode)
					|| S_ISLNK(st.st_mode)) {
					printf("%s\n", dent->d_name);
				}
			}
			closedir(dir);
		}
	} else {
		fprintf(stderr, "error: not a service or directory\n");
		exit_status = 1;
	}

	exit(exit_status);
}
