/*
** lib/misc.c
*/

/*
# license--start
#
# This file is part of the RUSS library.
# Copyright (C) 2012 John Marshall
#
# The RUSS library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# license--end
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Duplicate a string array.
*
* @param src	source string array
* @param copy_cnt	# of elements copied
* @param max_cnt	max # of elements supported
* @return	duplicated array
*/
char **
russ_dup_str_array(char **src, int *copy_cnt, int max_cnt) {
	char	**dst;
	int	i, cnt;

	if (src == NULL) {
		return NULL;
	}
	for (cnt = 0; (cnt < max_cnt) && (src[cnt] != NULL); cnt++);
	cnt++;

	if ((dst = malloc(sizeof(char *)*(cnt))) == NULL) {
		return NULL;
	}
	for (i = 0; i < cnt; i++) {
		if (src[i] == NULL) {
			dst[i] = NULL;
		} else if ((dst[i] = strdup(src[i])) == NULL) {
			goto free_dst;
		}
	}
	*copy_cnt = cnt;
	return dst;
free_dst:
	for (; i >= 0; i--) {
		free(dst[i]);
	}
	*copy_cnt = 0;
	return NULL;
}

/**
* fprintf-like but uses descripto instead of FILE *.
*
* @param fd	descriptor
* @param format	format string
* @return	# of bytes written
*/
int
russ_dprintf(int fd, char *format, ...) {
	char	buf[4096];
	int	n;
	va_list	ap;

	va_start(ap, format);
	n = vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);

	write(fd, buf, n);
	fsync(fd);
	return n;
}
