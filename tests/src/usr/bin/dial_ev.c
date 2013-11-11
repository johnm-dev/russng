/*
** dial_ev.c
*/

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include <russ.h>
#include <russ_experimental.h>

void
print_usage(char *prog_name) {
	fprintf(stderr, "usage: %s <op> <spath> [<arg> ...]\n\n"
		"Dial service and exit with the received exit value.\n", prog_name);
}

int
main(int argc, char **argv) {
	struct russ_buf	*rbufs[3];
	int	i, ev, rv, n;

	if ((argc < 3)
		|| ((argc == 1) 
			&& ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)))) {
		print_usage(basename(argv[0]));
		exit(1);
	}

#if 0
	/* create rbufs */
	if (((rbufs[0] = russ_buf_new(0)) == NULL)
		|| ((rbufs[1] = russ_buf_new(0)) == NULL)
		|| ((rbufs[2] = russ_buf_new(0)) == NULL)) {
		fprintf(stderr, "error: no memory\n");
		exit(1);
	}

	rv = russ_dialv_wait_inouterr(RUSS_DEADLINE_NEVER, argv[1], argv[2], NULL, &argv[3], &ev, rbufs);
#endif
	rv = russ_dialv_wait(RUSS_DEADLINE_NEVER, argv[1], argv[2], NULL, &argv[3], &ev);

	if (rv < 0) {
		fprintf(stderr, "error: dial failed\n");
		exit(1);
	}

	/* print captured output */
	printf("ev (%d)\n", ev);

	exit(0);
}