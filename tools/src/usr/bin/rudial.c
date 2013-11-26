/*
* rudial.c
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

#include "russ.h"

#define BUFSIZE		(1<<15)
#define BUFSIZE_MAX	(1<<20)

#ifdef USE_RUSS_RELAY2
#include "russ_relay2.h"
#endif

int
print_dir_list(char *spath) {
	struct stat		st;
	DIR			*dir;
	struct dirent		*dent;
	char			path[RUSS_REQ_SPATH_MAX];

	if ((dir = opendir(spath)) == NULL) {
		fprintf(stderr, "error: cannot open directory\n");
		return -1;
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
	return 0;
}

void
print_usage(char *prog_name) {
	if (strcmp(prog_name, "rudial") == 0) {
		printf(
"usage: rudial [<option>] <op> <spath> [<arg> ...]\n"
"\n"
"Dial service at <spath> to perform <op>. A service may support one\n"
"or more operations (e.g., execute, help, info, list).\n"
"\n"
"A successful dial will effectively connect the stdin, stdout, and\n"
"stderr of the service. Once connected, rudial forwards the stdin,\n"
"stdout, and sterr I/O data between the caller and the service.\n"
"\n"
"An exit value of < 0 indicates a failure to connect. Otherwise a 0\n"
"exit value is returned.\n"
);
	} else if (strcmp(prog_name, "ruexec") == 0) {
		printf( 
"usage: ruexec [<option>] <spath>\n"
"\n"
"Execute service at <spath>.\n"
);
	} else if (strcmp(prog_name, "ruhelp") == 0) {
		printf(
"usage: ruhelp [-t|--timeout <seconds>] <spath>\n"
"\n"
"Get help for service at <spath>.\n"
);
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		printf(
"usage: ruinfo [-t|--timeout <seconds>] <spath>\n"
"\n"
"Get information about service at <spath>.\n"
);
	} else if (strcmp(prog_name, "ruls") == 0) {
		printf(
"usage: ruls [<option>] <spath>\n"
"       ruls [-h|--help]\n"
"\n"
"List service(s) at <spath> (may also be a directory path).\n"
"Directory listings show service files, symlinks, and directories\n"
"only. Directories are indicated by a trailing / and the ./ entry\n"
"is always listed for a valid directory.\n"
);
	} else {
		return;
	}
	/* common help */
	printf(
"\n"
"Options:\n"
"-a|--attr <name=vaue>\n" \
"    Pass a 'name=value' string to the service.\n"
"-b <bufsize>\n" \
"    Set buffer size for reading/writing.\n"
"-t|--timeout <seconds>\n" \
"    Allow a given amount of time to connect before aborting.\n"
);
}

int
main(int argc, char **argv) {
	struct russ_cconn	*cconn;
	struct stat		st;
	russ_deadline		deadline;
	int			debug;
	int			timeout;
	char			*prog_name;
	char			*op, *spath, *arg;
	char			*attrv[RUSS_REQ_ATTRS_MAX];
	int			argi, attrc;
	int			bufsize, exit_status;

	signal(SIGPIPE, SIG_IGN);

	prog_name = basename(strdup(argv[0]));

	/* initialize */
	bufsize = BUFSIZE;
	debug = 0;
	deadline = RUSS_DEADLINE_NEVER;
	argi = 1;
	attrc = 0;
	attrv[0] = NULL;

	/* options */
	while (argi < argc) {
		arg = argv[argi++];

		if (strncmp(arg, "-", 1) != 0) {
			argi--;
			break;
		}

		if (((strcmp(arg, "--attr") == 0) || (strcmp(arg, "-a") == 0))
			&& (argi < argc)) {
			
			arg = argv[argi++];
			if (attrc >= RUSS_REQ_ATTRS_MAX-1) {
				fprintf(stderr, "error: too many attributes\n");
				exit(1);
			}
			if (strstr(arg, "=") == NULL) {
				fprintf(stderr, "error: bad attribute format\n");
				exit(1);
			}
			attrv[attrc++] = arg;
			attrv[attrc] = NULL;
		} else if ((strcmp(arg, "-b") == 0) && (argi < argc)) {
			arg = argv[argi++];
			if ((sscanf(arg, "%d", (int *)&bufsize) < 0)
				|| (bufsize <= 0)
				|| (bufsize > BUFSIZE_MAX)) {
				fprintf(stderr, "error: bad buffer size value\n");
				exit(1);
			}
		} else if (strcmp(arg, "--debug") == 0) {
			debug = 1;
		} else if ((strcmp(arg, "-h") == 0) || (strcmp(arg, "--help") == 0)) {
			print_usage(prog_name);
			exit(0);
		} else if (((strcmp(arg, "--timeout") == 0) || (strcmp(arg, "-t") == 0))
			&& (argi < argc)) {

			arg = argv[argi++];
			if (sscanf(arg, "%d", (int *)&timeout) < 0) {
				fprintf(stderr, "error: bad timeout value\n");
				exit(1);
			}
			timeout *= 1000;
			deadline = russ_to_deadline(timeout);
		} else {
			fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
			exit(1);
		}
	}

	/* [op], spath and args */
	if (argi == argc) {
		fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
		exit(1);
	}
	if ((strcmp(prog_name, "rudial") == 0) || (strcmp(prog_name, "ruexec") == 0)) {
		if ((strcmp(prog_name, "rudial") == 0) 
			&& (argi+2 <= argc)) {
			op = argv[argi++];
			spath = argv[argi++];
		} else if ((strcmp(prog_name, "ruexec") == 0)
			&& (argi+1 <= argc)) {
			op = "execute";
			spath = argv[argi++];
		} else {
			fprintf(stderr, "%s\n", RUSS_MSG_BAD_ARGS);
			exit(1);
		}
	} else if (strcmp(prog_name, "ruhelp") == 0) {
		op = "help";
		spath = argv[argi++];
	} else if (strcmp(prog_name, "ruinfo") == 0) {
		op = "info";
		spath = argv[argi++];
	} else if (strcmp(prog_name, "ruls") == 0) {
		op = "list";
		spath = argv[argi++];
		spath = russ_spath_resolve(spath);
	} else {
		fprintf(stderr, "error: unknown program name\n");
		exit(1);
	}

	exit_status = 0;
	if ((strcmp(op, "list") == 0) && (stat(spath, &st) == 0) && (!S_ISSOCK(st.st_mode))) {
		if (S_ISDIR(st.st_mode)) {
			exit_status = (print_dir_list(spath) == 0) ? 0 : 1;
		} else {
			fprintf(stderr, "error: not a service or directory\n");
			exit_status = 1;
		}
	} else {
		cconn = russ_dialv(deadline, op, spath, attrv, &(argv[argi]));
		if (cconn == NULL) {
			fprintf(stderr, "%s\n", RUSS_MSG_NO_DIAL);
			exit(RUSS_EXIT_CALL_FAILURE);
		}

#ifdef USE_RUSS_RELAY2
		{
			struct russ_relay2	*relay;
			relay = russ_relay2_new(3);
			russ_relay2_add(relay, STDIN_FILENO, cconn->fds[0], bufsize, 1);
			russ_relay2_add(relay, cconn->fds[1], STDOUT_FILENO, bufsize, 1);
			russ_relay2_add(relay, cconn->fds[2], STDERR_FILENO, bufsize, 1);

			cconn->fds[0] = -1;
			cconn->fds[1] = -1;
			cconn->fds[2] = -1;
			russ_relay2_serve(relay, -1, cconn->fds[3]);
			if (russ_cconn_wait(cconn, -1, &exit_status) < 0) {
				fprintf(stderr, "%s\n", RUSS_MSG_BAD_CONN_EVENT);
				exit_status = RUSS_EXIT_SYS_FAILURE;
			}
		}
#endif /* USE_RUSS_RELAY2 */

		russ_cconn_close(cconn);
		cconn = russ_cconn_free(cconn);
	}

	exit(exit_status);
}