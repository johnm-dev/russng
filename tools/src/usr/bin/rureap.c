/*
* rureap.c
*/

/*
# license--start
#
# Copyright 2018 John Marshall
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

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void
__reap_sigh(int signum) {
	static int	called = 0;

	if (called == 0) {
		kill(-getpgid(0), SIGTERM);
		called = 1;
	}
}

void
print_usage(char *prog_name) {
	printf(
"usage: rureap <pid> <path>\n"
"\n"
"Wait on the child process at <pid>. When it exits, reap it and\n"
"clean up the socket file at <path>.\n"
);
}

int
main(int argc, char **argv) {
	char	*path;
	int	pid;
	int	status, i;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv[0]);
		exit(0);
	} else if (argc != 3) {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(1);
	} else {
		if (sscanf(argv[1], "%d", &pid) != 1) {
			fprintf(stderr, "error: bad/missing arguments\n");
			exit(1);
		}
		path = argv[2];
	}

	/* close and reopen to occupy fds 0-2 */
	for (i = 0; i < 1024; i++) {
		close(i);
	}
	open("/dev/null", O_WRONLY);
	open("/dev/null", O_RDONLY);
	open("/dev/null", O_RDONLY);


	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, __reap_sigh);
	signal(SIGINT, __reap_sigh);
	signal(SIGTERM, __reap_sigh);
	signal(SIGQUIT, __reap_sigh);

	waitpid(pid, &status, 0);
	remove(path);
	exit(0);
}
