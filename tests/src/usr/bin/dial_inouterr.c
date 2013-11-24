/*
** dial_inouterr.c
*/

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>

#include <russ.h>
#include <russ_experimental.h>

void
print_usage(char *prog_name) {
	fprintf(stderr, "usage: %s [-] <op> <spath> [<arg> ...]\n\n"
		"Dial service and capture connection stdout and stderr\n"
		"If '-' is specified, capture stdin first (use CTRL-D to\n"
		"complete input).\n", prog_name);
}

int
main(int argc, char **argv) {
	struct russ_buf	*rbufs[3];
	int	i, ev, rv, n, argi;

	if ((argc < 3)
		|| ((argc == 1) 
			&& ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)))) {
		print_usage(basename(argv[0]));
		exit(1);
	}

	/* create rbufs */
	if (((rbufs[0] = russ_buf_new(1<<20)) == NULL)
		|| ((rbufs[1] = russ_buf_new(1<<20)) == NULL)
		|| ((rbufs[2] = russ_buf_new(1<<20)) == NULL)) {
		fprintf(stderr, "error: no memory\n");
		exit(1);
	}

	argi = 1;
	if (strcmp(argv[argi], "-") == 0) {
		/* load stdin rbuf */
		while (1) {
			n = rbufs[0]->cap-rbufs[0]->len;
			if ((n == 0) || ((n = russ_read(0, &rbufs[0]->data[rbufs[0]->len], n)) <= 0)) {
				break;
			}
			rbufs[0]->len += n;
		}
		argi++;
	}

	rv = russ_dialv_wait_inouterr(RUSS_DEADLINE_NEVER, argv[argi], argv[argi+1], NULL, &argv[argi+2], &ev, rbufs);

	//rv = russ_dialv_wait_inouterr3(RUSS_DEADLINE_NEVER, argv[argi], argv[argi+1], NULL, &argv[argi+2],
	//		&ev, rbufs[0], rbufs[1], rbufs[2]);

	if (rv < 0) {
		fprintf(stderr, "error: dial failed\n");
		exit(1);
	}

	/* print captured output */
	printf("ev (%d)\n", ev);
	printf("out: cap (%d) len (%d) off (%d)\n", rbufs[1]->cap, rbufs[1]->len, rbufs[1]->off);
	printf("data\n----------\n");
	if (rbufs[1]->len) {
		printf("%s", rbufs[1]->data);
	}
	printf("----------\n");

	printf("\n");
	printf("err: cap (%d) len (%d) off (%d)\n", rbufs[2]->cap, rbufs[2]->len, rbufs[2]->off);
	printf("data\n----------\n");
	if (rbufs[2]->len) {
		printf("%s", rbufs[2]->data);
	}
	printf("----------\n");

	exit(0);
}
