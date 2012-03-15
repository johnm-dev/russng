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

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Resolve addr to be absolute.
*
* @param addr	service address
* @return	absolute path address
*/
char *
russ_resolve_addr(char *addr) {
	char	*addr2;
	char	*service_dir;

	if (strstr(addr, ":") == addr) {
		if ((service_dir = getenv("RUSS_SERVICE_DIR")) == NULL) {
			service_dir = RUSS_SERVICE_DIR;
		}
		if ((addr2 = malloc(strlen(service_dir)+1+strlen(addr)+1)) == NULL) {
			return NULL;
		}
		sprintf(addr2, "%s/%s\0", service_dir, addr);
	} else {
		return strdup(addr);
	}
	return addr2;
}

/**
* Find socket address from full address.
*
* @param addr	full service address
* @return	socket address; NULL on failure
*/
char *
russ_find_service_addr(char *addr) {
	char		*saddr;
	struct stat	st;

	if ((addr = russ_resolve_addr(addr)) != NULL) {
		saddr = strdup(addr);
		while (stat(saddr, &st) != 0) {
			saddr = dirname(saddr);
		}
		if (S_ISSOCK(st.st_mode)) {
			free(addr);
			return saddr;
		}
		free(addr);
		free(saddr);
	}
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
