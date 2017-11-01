/*
* ruspawn.c
*/

/*
# license--start
#
# Copyright 2017 John Marshall
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <russ/russ.h>

void
print_usage(char *prog_name) {
	printf(
"usage: ruspawn (-f <path>|-c <name>=<value>) [...] [-- ...]\n"
"\n"
"Spawn a russ server using the configuration, outputting the path of\n"
"a dynamically created socket file. The listener socket is passed to\n"
"the server.\n"
"\n"
"The lifetime of the server is determined by the configuration\n"
"settings main:closeonaccept and main:accepttimeout.\n"
"\n"
"Where:\n"
"-c <name>=<value>\n"
"	Set configuration attribute.\n"
"-f <path>\n"
"	Load configuration file.\n"
"-- ...	Arguments to pass to the server program.\n"
);
}

struct russ_conf	*conf = NULL;

int
main(int argc, char **argv) {
	char	*saddr;

	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv[0]);
		exit(0);
	}
	if ((saddr = russ_spawn(argc, argv)) == NULL) {
		fprintf(stderr, "error: cannot spawn server\n");
		exit(1);
	}
	printf("%s", saddr);
	exit(0);
}
