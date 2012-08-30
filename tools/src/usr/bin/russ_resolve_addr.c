/*
* addr-test.c
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
	printf("usage: %s <addr> [...]\n", basename(prog_name));
	free(prog_name);
}

int
main(int argc, char **argv) {
	char	*res_addr;
	int	i;

	if (argc == 1) {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(-1);
	} else if ((argc == 2)
		&& ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0))) {
		print_usage(argv);
		exit(0);
	}

	for (i = 1; i < argc; i++) {
		res_addr = russ_resolve_addr(argv[i]);
		printf("addr (%s)\nresolved addr (%s)\n\n", argv[i], res_addr);
	}
	exit(0);
}
