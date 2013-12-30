/*
* str_test.c
*/

#include <stdio.h>
#include <stdlib.h>

#include <russ.h>

int
main(int argc, char **argv) {
	char	*spath = "/a/bc/def";
	char	*s;
	int	i;

	for (i = 0; i < 10; i++) {
		s = russ_str_dup_comp(spath, '/', i);
		if (s == NULL) {
			break;
		}
		printf("spath (%s) i (%d) s (%s)\n", spath, i, s);
		free(s);
	};
}
