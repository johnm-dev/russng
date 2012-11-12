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
#include <pwd.h>
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
* RUSS_SERVICES_DIR (from env or C #define). Prefixes /++ and ++ are
* resolved as equivalent to the "$HOME/.russ" of the user identified
* by uid. Symlinks are resolved by reading the link rather than
* following them via the OS. This allows one to use symlinks that
* use the above prefixes and also which actually do not exist in the
* filesystem (i.e., for referencing non-local, valid russ
* addresses).
*
* Note: if uid == NULL, then /++ and ++ prefixes are not resolved
* and the return value is NULL (failure).
*
* @param addr		service address
* @param uid		pointer to uid (may be NULL)
* @return		absolute path (malloc'ed); NULL on failure
*/
char *
russ_resolve_addr_uid(char *addr, uid_t *uid_p) {
	struct stat	st;
	char		buf[RUSS_REQ_PATH_MAX], lnkbuf[RUSS_REQ_PATH_MAX], tmpbuf[RUSS_REQ_PATH_MAX];
	char		*bp, *bend, *bp2;
	char		*sfmt, *lfmt;
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

	/* special case */
	if ((strcmp(buf, "+") == 0) || (strcmp(buf, "++") == 0)) {
		strcat(buf, "/");
	}

	/*
	* TODO: the following code could be simplified and
	* clarified so that flow is obvious.
	*/
	changed = 1;
	while (changed) {
		changed = 0;
		if ((strstr(buf, "+/") == buf) || (strstr(buf, "/+/") == buf)) {
			/* resolve prefixes */
			if (buf[0] == '+') {
				bp = &buf[2];
			} else {
				bp = &buf[3];
			}
			if ((snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", services_dir, bp) < 0)
				|| (strncpy(buf, tmpbuf, sizeof(buf)) < 0)) {
				return NULL;
			}
			changed = 1;
		} else if ((strstr(buf, "++/") == buf) || (strstr(buf, "/++/") == buf)) {
			struct passwd	pwd, *result;
			char		pwd_buf[16384];

			if (buf[0] == '+') {
				bp = &buf[3];
			} else {
				bp = &buf[4];
			}
			if ((uid_p == NULL)
				|| (getpwuid_r(*uid_p, &pwd, pwd_buf, sizeof(pwd_buf), &result) != 0)
				|| (result == NULL)
				|| (snprintf(tmpbuf, sizeof(tmpbuf), "%s/.russ/%s", pwd.pw_dir, bp) < 0)
				|| (strncpy(buf, tmpbuf, sizeof(buf)) < 0)) {
				return NULL;
			}
			changed = 1;
		} else if (buf[0] != '\0') {
			/* for each subpath, test for symlink and resolve */
			bp = buf;
			while (bp != NULL) {
				if ((bp = index(bp+1, '/')) != NULL) {
					*bp = '\0'; /* delimit path to check */
				}
				if (lstat(buf, &st) == 0) {
					if (S_ISDIR(st.st_mode)) {
						if (bp != NULL) {
							*bp = '/'; /* restore */
						}
						continue;
					} else if (S_ISLNK(st.st_mode)) {
						if (readlink(buf, lnkbuf, sizeof(lnkbuf)) < 0) {
							/* insufficient space */
							return NULL;
						}
						lnkbuf[st.st_size] = '\0';

						if ((lnkbuf[0] == '/') || (strncmp(lnkbuf, "+/", 2) == 0)) {
							/* replace subpath with lnkbuf */
							snprintf(tmpbuf, sizeof(tmpbuf), "%s", lnkbuf);
						} else {
							if ((bp2 = rindex(buf, '/')) != NULL) {
								/* append lnkbuf to subpath */
								*bp2 = '\0';
								snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", buf, lnkbuf);
								*bp2 = '/';
							} else {
								/* replace single component subpath with lnkbuf */
								snprintf(tmpbuf, sizeof(tmpbuf), "%s", lnkbuf);
							}
						}
						if (bp != NULL) {
							/* append path right of subpath */
							*bp = '/';
							strncat(tmpbuf, bp, sizeof(tmpbuf));
						}
						/* copy back to buf */
						if (snprintf(buf, sizeof(buf), "%s", tmpbuf) < 0) {
							return NULL;
						}
						changed = 1;
						break;
					}
					/* stop; not a dir nor a symlink */
				} 
				if (bp != NULL) {
					*bp = '/'; /* restore / */
				}
				break;
			}
		}
	}
	return strdup(buf);
}

/**
* Simple case for russ_resolve_addr_uid without current uid.
*/
char *
russ_resolve_addr(char *addr) {
	uid_t	uid;
	uid = getuid();
	return russ_resolve_addr_uid(addr, &uid);
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
* Find service target (saddr and spath).
*
* A service address is composed of a socket address (saddr) and a
* service path (spath). The saddr is local, the spath is passed to
* the service server. Depending on the service server, the spath may
* also be a service address and need to be resolved and followed.
*
* @param addr		full service address
* @return		russ_target object; NULL on failure
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
	if ((strncpy(targ->saddr, addr, RUSS_REQ_PATH_MAX) < 0)
		|| (snprintf(targ->spath, RUSS_REQ_PATH_MAX, "/%s", p) < 0)) {
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
