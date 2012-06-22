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
* Resolve addr by replacing prefixes and symlinks.
*
* Prefixes of /+ and + are resolved as equivalent to
* RUSS_SERVICES_DIR (env or constant). Symlinks are resolved without
* actually following them--this allows one to use symlinks that
* use the above prefixes and which actually do not exist in the
* filesystem.
*
* @param addr	service address
* @return	absolute path (malloc'ed), NULL on failure
*/
char *
russ_resolve_addr(char *addr) {
	struct stat	st;
	char		buf[8192], tmpbuf[8192];
	char		*bp, *bend;
	char		*services_dir;
	int		sdlen, cnt, stval;
	int		changed;

	if (strncpy(buf, addr, sizeof(buf)) < 0) {
		return NULL;
	}
	if ((services_dir = getenv("RUSS_SERVICES_DIR")) == NULL) {
		services_dir = RUSS_SERVICES_DIR;
	}
	sdlen = strlen(services_dir);
	bend = buf+sizeof(buf);

	/* TODO: the follow code needs work to simplify and clarify
	* flow paths and why
	*/
	changed = 1;
	while (changed) {
		changed = 0;
		if ((strstr(buf, "+") == buf) || (strstr(buf, "/+") == buf)) {
			/* resolve prefixes */
			if (buf[0] == '+') {
				bp = &buf[1];
			} else {
				bp = &buf[2];
			}
			if ((snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", services_dir, bp) < 0)
				|| (strncpy(buf, tmpbuf, sizeof(buf)) < 0)) {
				return NULL;
			}
			changed = 1;
		} else if (buf[0] != '\0') {
			/* resolve _a_ symlink, if referenced */
			bp = buf;
			while (bp != NULL) {
				/* resolve _a_ symlink, if referenced */
				if ((bp = index(bp+1, '/')) != NULL) {
					*bp = '\0'; /* delimit path to check */
				}
				if (lstat(buf, &st) == 0) {
					if (S_ISLNK(st.st_mode)) {
						if (readlink(buf, tmpbuf, sizeof(tmpbuf)) < 0) {
							/* insufficient space */
							return NULL;
						}
						tmpbuf[st.st_size] = '\0';
						if ((bp != NULL) 
							&& (snprintf(tmpbuf+st.st_size, sizeof(tmpbuf)-st.st_size, "/%s", bp+1) < 0)) {
							/* insufficient space */
							return NULL;
						}
						if (strncpy(buf, tmpbuf, sizeof(buf)) < 0) {
							/* insufficient space */
							return NULL;
						}
						changed = 1;
						break;
					}
					if (bp != NULL) {
						*bp = '/'; /* restore / */
					}
					if (!S_ISDIR(st.st_mode)) {
						break;
					}
				} else {
					if (bp != NULL) {
						*bp = '/'; /* restore / */
					}
					break;
				}
			}
		}
	}
	return strdup(buf);
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
* Find service target which is the saddr (socket address) and the
* spath (service path).
*
* @param addr	full service address
* @return	russ_target object
*/
struct russ_target *
russ_find_service_target(char *addr) {
	struct russ_target	*targ;
	struct stat		st;
	char			*p;

	/* must be non-NULL, non-empty string; return value must be free */
	if (((addr = russ_resolve_addr(addr)) == NULL)
		|| (addr[0] == '\0')) {
		return NULL;
	}

	/*
	* search for socket file in addr as sub-paths (from left to
	* right); temporarily replacing '/' with '\0' avoids
	* needless copying
	*/
	p = addr;
	while (p != NULL) {
		if ((p = index(p+1, '/')) != NULL) {
			*p = '\0';
		}
		if (lstat(addr, &st) == 0) {
			if (S_ISSOCK(st.st_mode)) {
				/* found socket; position p to end or next char */
				if (p == NULL) {
					p = "\0";
				} else {
					p++;
				}
				break;
			} else if (!S_ISDIR(st.st_mode)) {
				/* non-socket file */
				return NULL;
			}
		}
		/* restore '/' if possible */
		if (p != NULL) {
			*p = '/';
		}
	}

	/* allocate and initialize */
	if ((targ = malloc(sizeof(struct russ_target))) == NULL) {
		goto free_addr;
	}

	/* copy into target */
	if ((strncpy(targ->saddr, addr, RUSS_MAX_SPATH_LEN) < 0)
		|| (snprintf(targ->spath, RUSS_MAX_SPATH_LEN, "/%s", p) < 0)) {
		goto free_targ;
	}
	free(addr);
	return targ;

free_targ:
	free(targ);
free_addr:
	free(addr);
	return NULL;
}
