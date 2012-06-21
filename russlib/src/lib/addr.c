/*
** lib/addr.c
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
* Resolve addr. Special prefixes /+ and + are resolved as equivalent
* RUSS_SERVICES_DIR (env or constant).
*
* @param addr	service address
* @return	absolute path address
*/
char *
russ_resolve_addr(char *addr) {
	char	*addr2;
	char	*services_dir;

	if ((strstr(addr, "+") == addr) || (strstr(addr, "/+") == addr)) {
		if (addr[0] == '+') {
			addr = &addr[1];
		} else {
			addr = &addr[2];
		}
		if ((services_dir = getenv("RUSS_SERVICES_DIR")) == NULL) {
			services_dir = RUSS_SERVICES_DIR;
		}
		if ((addr2 = malloc(strlen(services_dir)+1+strlen(addr)+1)) == NULL) {
			return NULL;
		}
		sprintf(addr2, "%s/%s\0", services_dir, addr);
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
* Find service target.
*
* @param path	full path to service
* @return	russ_target object
*/
struct russ_target *
russ_find_service_target(char *path) {
	struct russ_target	*targ;
	struct stat		st;
	char			saddr[RUSS_MAX_SPATH_LEN],
				spath[RUSS_MAX_SPATH_LEN],
				tmps[RUSS_MAX_SPATH_LEN];
	char			*bname;
	int			cnt;

	/* TODO:
		should targ be allocated at end?
		should targ members be used for work?
	*/
	/* allocate and initialize */
	if ((targ = malloc(sizeof(struct russ_target))) == NULL) {
		return NULL;
	}
	targ->saddr[0] = '\0';
	targ->spath[0] = '\0';
	tmps[RUSS_MAX_SPATH_LEN-1] = '\0';

	/* initialize working var */
	saddr[sizeof(saddr)] = '\0';
	if (strncpy(saddr, path, sizeof(saddr)-1) < 0) {
		goto free_targ;
	};

	/* resolve addr to target */
	while (1) {
		if (lstat(saddr, &st) == 0) {
			if (!S_ISLNK(st.st_mode)) {
				/* non-symlink object */
				break;
			}
			if ((cnt = readlink(saddr, tmps, sizeof(tmps)-1)) < 0) {
				/* no space */
				goto free_targ;
			}
			strncpy(saddr, tmps, st.st_size);
			saddr[cnt] = '\0';
		}
		/* prepend component, update saddr */
		bname = basename(saddr);
		if (strcmp(bname, saddr) == 0) {
			/* failed */
			goto free_targ;
		}
		if (((cnt = snprintf(tmps, sizeof(tmps)-1, "/%s%s", bname, spath)) < 0)
			|| (cnt >= sizeof(tmps)-1)) {
			/* no space */
			goto free_targ;
		}
		strcpy(spath, tmps);
		dirname(saddr);
	}

	/* copy into target */
	strcpy(targ->saddr, saddr);
	strcpy(targ->spath, spath);
	return targ;

free_targ:
	free(targ);
	return NULL;
}
