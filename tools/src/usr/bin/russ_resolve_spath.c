/*
* russ_resolve_spath.c
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

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "russ.h"

void
print_usage(char **argv) {
	char	*prog_name;

	prog_name = strdup(argv[0]);
	printf("usage: %s <spath> [...]\n", basename(prog_name));
	free(prog_name);
}

int
main(int argc, char **argv) {
	char	*res_spath;
	int	i;

	if (argc == 1) {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(1);
	} else if ((argc == 2)
		&& ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
		print_usage(argv);
		exit(0);
	}

	for (i = 1; i < argc; i++) {
		res_spath = russ_spath_resolve(argv[i]);
		printf("spath (%s)\nresolved spath (%s)\n\n", argv[i], res_spath);
	}
	exit(0);
}
