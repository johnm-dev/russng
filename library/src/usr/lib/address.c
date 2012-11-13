/*
** lib/address.c
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
* Resolve spath by replacing prefixes and symlinks.
*
* Prefixes of /+ and + are resolved as equivalent to
* RUSS_SERVICES_DIR (from env or C #define). Prefixes /++ and ++ are
* resolved as equivalent to the "$HOME/.russ" of the user identified
* by uid. Symlinks are resolved by reading the link rather than
* following them via the OS. This allows one to use symlinks that
* use the above prefixes and also which actually do not exist in the
* filesystem (i.e., for referencing non-local, valid russ service
* paths).
*
* Note: if uid == NULL, then /++ and ++ prefixes are not resolved
* and the return value is NULL (failure).
*
* @param spath		service path
* @param uid		pointer to uid (may be NULL)
* @return		absolute path (malloc'ed); NULL on failure
*/
char *
russ_resolve_spath_uid(char *spath, uid_t *uid_p) {
	struct stat	st;
	char		buf[RUSS_REQ_SPATH_MAX], lnkbuf[RUSS_REQ_SPATH_MAX], tmpbuf[RUSS_REQ_SPATH_MAX];
	char		*bp, *bend, *bp2;
	char		*sfmt, *lfmt;
	char		*services_dir;
	int		sdlen, cnt, stval;
	int		changed;

	if (strncpy(buf, spath, sizeof(buf)) < 0) {
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
* Simple case for russ_resolve_spath_uid without current uid.
*/
char *
russ_resolve_spath(char *spath) {
	uid_t	uid;
	uid = getuid();
	return russ_resolve_spath_uid(spath, &uid);
}

/**
* Find socket address from service path.
*
* @param spath	full service path
* @return	socket address; NULL on failure
*/
char *
russ_find_socket_addr(char *spath) {
	char		*saddr;
	struct stat	st;

	if ((spath = russ_resolve_spath(spath)) != NULL) {
		saddr = strdup(spath);
		while (stat(saddr, &st) != 0) {
			saddr = dirname(saddr);
		}
		if (S_ISSOCK(st.st_mode)) {
			free(spath);
			return saddr;
		}
		free(spath);
		free(saddr);
	}
	return NULL;
}

/**
* Find service target: saddr and (remaining) spath.
*
* A service target is composed of a socket address (saddr) and a
* service path (spath). The saddr is local, the spath is passed to
* the service server. Depending on the service server, the spath may
* also be a service target and need to be resolved and followed.
*
* @param spath		full service path
* @return		russ_target object; NULL on failure
*/
struct russ_target *
russ_find_service_target(char *spath) {
	struct russ_target	*targ;
	struct stat		st;
	char			*p;

	/* must be non-NULL, non-empty string; return value must be free */
	if (((spath = russ_resolve_spath(spath)) == NULL)
		|| (spath[0] == '\0')) {
		return NULL;
	}

	/*
	* search for socket file in spath as sub-paths (from left to
	* right); temporarily replacing '/' with '\0' avoids
	* needless copying
	*/
	p = spath;
	while (p != NULL) {
		if ((p = index(p+1, '/')) != NULL) {
			*p = '\0';
		}
		if (lstat(spath, &st) == 0) {
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
		goto free_spath;
	}

	/* copy into target */
	if ((strncpy(targ->saddr, spath, RUSS_REQ_SPATH_MAX) < 0)
		|| (snprintf(targ->spath, RUSS_REQ_SPATH_MAX, "/%s", p) < 0)) {
		goto free_targ;
	}
	free(spath);
	return targ;

free_targ:
	free(targ);
free_spath:
	free(spath);
	return NULL;
}
