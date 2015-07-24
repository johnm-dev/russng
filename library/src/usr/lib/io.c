/*
* lib/io.c
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

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* fprintf-like for descriptor instead of FILE *.
*
* @param fd		descriptor
* @param format		printf-style format string
* @param ...		variadic list of arguments
* @return		# of bytes written; -1 on error
*/
int
russ_dprintf(int fd, const char *format, ...) {
	va_list	ap;
	int	rv;

	va_start(ap, format);
	rv = russ_vdprintf(fd, format, ap);
	va_end(ap);
	return rv;
}

/**
* fprintf-like for filename instead of FILE *.
*
* @param path		filename
* @param dformat	strftime-style format string
* @param format		printf-style format string
* @param ...		variadic list of arguments
* @return		# of bytes written; -1 on error
*/
int
russ_lprintf(const char *path, const char *dformat, const char *format, ...) {
	va_list	ap;
	FILE	*f;
	int	rv;

	if ((f = fopen(path, "a")) == NULL) {
		return -1;
	}
	if (dformat) {
		char		buf[128];
		time_t		t;
		struct tm	*tm;

		t = time(NULL);
		tm = localtime(&t);
		if (strftime(buf, sizeof(buf), dformat, tm)) {
			fprintf(f, "%s", buf);
		}
	}
	va_start(ap, format);
	rv = vfprintf(f, format, ap);
	va_end(ap);
	fflush(f);
	fclose(f);
	return rv;
}

/**
* va_list-based fprintf-like for descriptor instead of FILE *.
*
* @param fd		descriptor
* @param format		printf-style format string
* @param ap		va_list of arguments
* @return		# of bytes written; -1 on error
*/
int
russ_vdprintf(int fd, const char *format, va_list ap) {
	va_list	aq;
	char	_buf[8192];
	char	*buf;
	int	n, bufsz;

	buf = _buf;
	bufsz = sizeof(_buf);
	while (1) {
		va_copy(aq, ap);
		n = vsnprintf(buf, bufsz, format, aq);
		va_end(aq);
		if (n < 0) {
			goto free_buf;
		} else if (n < bufsz) {
			break;
		}

		/* allocate */
		bufsz = n+1; /* include \0 */
		if ((buf = malloc(bufsz)) == NULL) {
			goto free_buf;
		}
	}
	if (russ_writen(fd, buf, n) < n) {
		n = -1;
	}
	/* fallthrough */
free_buf:
	if (buf != _buf) {
		buf = russ_free(buf);
	}
	return n;
}
