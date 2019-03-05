
/*
** russ_str_resolve.c
**
** gcc -o russ_str_resolve russ_str_resolve.c ../wip-6.8/library/src/usr/lib/libruss.a
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <russ/russ.h>

void
print_usage() {
	printf(
"usage: russ_str_resolve [[<name>=<value>] ...] <fmt>\n"
"       russ_str_resolve -h|--help\n"
"\n"
"Resolve fmt string using provided names and values.\n"
"\n"
"In the fmt, strings of the form ${name} are replaced by the\n"
"corresponding value.\n");
}

int
main(int argc, char **argv) {
	char	**vars;
	char	*fmt, *s, *eqsign;
	int	i;


	if (argc < 2) {
		fprintf(stderr, "error: bad/missing arguments\n");
		exit(1);
	}
	if (argc == 2) {
		if ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0)) {
			print_usage();
			exit(0);
		}
	}

	/* collect variables */
	vars = russ_sarray0_new(0);
	for (i = 1; i < argc-1; i++) {
		s = argv[i];

		if ((eqsign = strchr(s, '=')) == NULL) {
			/* invalid, ignore */
			continue;
		}

		/* TODO: support find and replace */
		if (russ_sarray0_append(&vars, s, NULL) < 0) {
			fprintf(stderr, "error: cannot add variable\n");
			exit(0);
		}
	}
	//fprintf(stderr, "count (%d)\n", russ_sarray0_count(vars, 100));
	//for (i = 0; vars[i] != NULL; i++) {
		//fprintf(stderr, "[%d] (%s)\n", i, vars[i]);
	//}

	/* resolve fmt */
	fmt = argv[argc-1];
	//fprintf(stderr, "fmt (%s)\n", fmt);
	fmt = russ_str_resolve(fmt, vars);

	/* output result */
	if (fmt == NULL) {
		fprintf(stderr, "error: bad format string\n");
		exit(1);
	}
	printf("%s", fmt);
	exit(0);
}
