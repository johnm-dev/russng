/*
* snprintf_test.c
*/

#include <stdio.h>

void
print_codes(char *s, int len) {
	int	i;

	for (i = 0; i < len; i++) {
		printf("char (%c) code (%d)\n",
			isprint(s[i]) ? s[i] : '?',
			s[i]);
	}
	printf("\n");
}

int
main(int argc, char **argv) {
	char	buf[4];

	printf("exit (%d)\n", snprintf(buf, sizeof(buf), ""));
	print_codes(buf, sizeof(buf));

	printf("exit (%d)\n", snprintf(buf, sizeof(buf), "0"));
	print_codes(buf, sizeof(buf));

	printf("exit (%d)\n", snprintf(buf, sizeof(buf), "01"));
	print_codes(buf, sizeof(buf));

	printf("exit (%d)\n", snprintf(buf, sizeof(buf), "012"));
	print_codes(buf, sizeof(buf));

	printf("exit (%d)\n", snprintf(buf, sizeof(buf), "0123"));
	print_codes(buf, sizeof(buf));

	printf("exit (%d)\n", snprintf(buf, sizeof(buf), "01234"));
	print_codes(buf, sizeof(buf));
}
