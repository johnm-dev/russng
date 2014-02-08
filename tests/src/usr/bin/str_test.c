/*
* str_test.c
*/

#include <stdio.h>
#include <stdlib.h>

#include <russ.h>

void
test_russ_str_dup_comp(void) {
	char	*spath = "/a/bc/def";
	char	*s;
	int	i;

	printf("test_russ_str_dup_comp():\n");
	for (i = 0; i < 10; i++) {
		s = russ_str_dup_comp(spath, '/', i);
		if (s == NULL) {
			break;
		}
		printf("spath (%s) i (%d) s (%s)\n", spath, i, s);
		s = russ_free(s);
	};
}

void
test_russ_str_get_comp(void) {
	char	*spath = "/a/bc/def";
	char	buf[3];
	int	i;

	printf("test_russ_str_get_comp():\n");
	for (i = 0; i < 10; i++) {
		if (russ_str_get_comp(spath, '/', i, buf, sizeof(buf)) < 0) {
			break;
		}
		printf("spath (%s) i (%d) buf (%s)\n", spath, i, buf);
	}
}

int
main(int argc, char **argv) {
	test_russ_str_dup_comp();
	test_russ_str_get_comp();
}
