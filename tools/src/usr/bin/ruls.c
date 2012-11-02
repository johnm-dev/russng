/*
* ruls.c
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

#include <dirent.h>
#include <libgen.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "russ.h"

void
print_usage(char *prog_name) {
	printf(
"usage: ruls [-t|--timeout <seconds>] <addr>|<path>\n"
"       ruls [-h|--help]\n"
"\n"
"List service(s) at <addr> or a directory entries at <path>.\n"
"Directory listings show service files and directories only.\n"
"Directories are indicated by a trailing / and the ./ entry is\n"
"always listed for a valid directory.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_conn	*conn;
	struct russ_forwarder	fwds[RUSS_CONN_NFDS];
	struct stat		st;
	char			*prog_name;
	char			*addr;
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
		addr = argv[1];
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
		addr = argv[3];
	} else {
		addr = ".";
	}

	/* resolve before calling russ_list() */
	addr = russ_resolve_addr(addr);

	/* TODO: clean up exit_status usage */

	/* call russ_list() only if a socket file; otherwise list dir */
	exit_status = 0;
	if ((stat(addr, &st) < 0) || S_ISSOCK(st.st_mode)) {
		conn = russ_list(deadline, addr);

		if (conn == NULL) {
			fprintf(stderr, "%s\n", RUSS_MSG_NO_DIAL);
			exit(RUSS_EXIT_CALL_FAILURE);
		}

		/* initialize forwarders (handing off fds) and start threads */
		russ_forwarder_init(&(fwds[0]), 0, STDIN_FILENO, conn->fds[0], -1, 16384, 0, 1);
		russ_forwarder_init(&(fwds[1]), 0, conn->fds[1], STDOUT_FILENO, -1, 16384, 0, 1);
		russ_forwarder_init(&(fwds[2]), 0, conn->fds[2], STDERR_FILENO, -1, 16384, 0, 1);
		conn->fds[0] = -1;
		conn->fds[1] = -1;
		conn->fds[2] = -1;
		if (russ_run_forwarders(RUSS_CONN_STD_NFDS-1, fwds) < 0) {
			fprintf(stderr, "error: could not forward bytes\n");
			exit(RUSS_EXIT_SYS_FAILURE);
		}

		/* wait for exit */
		if (russ_conn_wait(conn, &exit_status, -1) < 0) {
			fprintf(stderr, "error: unexpected connection event\n");
			exit_status = RUSS_EXIT_SYS_FAILURE;
		}
		russ_forwarder_join(&(fwds[1]));

		russ_conn_close(conn);
		conn = russ_conn_free(conn);
	} else if (S_ISDIR(st.st_mode)) {
		DIR		*dir;
		struct dirent	*dent;
		char		path[RUSS_MAX_PATH_LEN];

		if ((dir = opendir(addr)) == NULL) {
			fprintf(stderr, "error: cannot open directory\n");
			exit_status = 1;
		} else {
			while ((dent = readdir(dir)) != NULL) {
				if (strcmp(dent->d_name, "..") == 0) {
					continue;
				}
				if ((snprintf(path, sizeof(path), "%s/%s", addr, dent->d_name) < 0)
					|| (stat(path, &st) < 0)) {
					/* problem */
					continue;
				}
				if (S_ISDIR(st.st_mode)) {
					printf("%s/\n", dent->d_name);
				} else if (S_ISSOCK(st.st_mode)) {
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
