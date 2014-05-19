/*
* rustart.c
*/

/*
# license--start
#
# Copyright 2014 John Marshall
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

#include <russ_priv.h>

void
print_usage(char *prog_name) {
	printf(
"usage: rustart (-f <path>|-c <name>=<value>) [...] [-- ...]\n"
"\n"
"Start a uss server. Using the configuration, a socket file is\n"
"created and the listener socket is passed to the server.\n"
"\n"
"Where:\n"
"-f <path>\n"
"	Load configuration file.\n"
"-c <name>=<value>\n"
"	Set configuration attribute.\n"
"-- ...	Arguments to pass to the server program.\n"
);
}

struct russ_conf	*conf = NULL;

int
main(int argc, char **argv) {
	if ((argc == 2) && (strcmp(argv[1], "-h") == 0)) {
		print_usage(argv[0]);
		exit(0);
	}
	signal(SIGPIPE, SIG_IGN);
	russ_start(argc, argv);
	fprintf(stderr, "error: cannot start server\n");
	exit(1);
}
