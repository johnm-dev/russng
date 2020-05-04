/*
* lib/spath.c
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

#include "russ/priv.h"

#define RUSS_SPATH_RESOLVE_SYMLINKS_MAX	32

/**
* Test for option (look for "?").
*
* @param spath		service path
* @return		0 on none, 1 on at least one
*/
int
russ_spath_hasoption(const char *spath) {
	const char	*last;

	if (spath == NULL) {
		return 0;
	}
	if ((last = rindex(spath, '/')) == NULL) {
		last = spath;
	}
	if (index(last, '?') == NULL) {
		return 0;
	}
	return 1;
}

/**
* Get last component from spath.
*
* Caller must free value.
*
* @param spath		service path
* @return		last component string; NULL otherwise
*/
char *
russ_spath_getlast(const char *spath) {
	char	*last;

	if (spath == NULL) {
		return NULL;
	}
	if ((last = rindex(spath, '/')) == NULL) {
		return strdup(spath);
	}
	return strdup(last+1);
}

/**
* Return spath service name of last component.
*
* Caller must free value.
*
* @param spath		service path
* @return		name from last component, NULL otheriwse
*/
char *
russ_spath_getname(const char *spath) {
	const char	*name;
	char		*qsep;

	if (spath == NULL) {
		return NULL;
	}
	if ((name = rindex(spath, '/')) == NULL) {
		name = spath;
	} else {
		name++;
	}
	qsep = index(name, '?');
	if (qsep == NULL) {
		return strdup(name);
	}
	return strndup(name, qsep-name);
}

/**
* Return options.
*
* @param spath		service path
* @return		arrary of options; NULL otherwise
*/
char **
russ_spath_getoptions(const char *spath) {
	const char 	*last;

	if (spath == NULL) {
		return NULL;
	}
	if ((last = rindex(spath, '/')) == NULL) {
		last = spath;
	}
	return russ_sarray0_new_split((char *)last, "?", 1);
}

/**
* Resolve spath by replacing symlinks.
*
* @param spath		service path
* @param uid		pointer to uid (may be NULL)
* @param follow		follow symlinks (0 to disable)
* @return		absolute path (malloc'ed); NULL on failure
*/
char *
russ_spath_resolvewithuid(const char *spath, uid_t *uid_p, int follow) {
	struct stat	st;
	char		buf[RUSS_REQ_SPATH_MAX], lnkbuf[RUSS_REQ_SPATH_MAX], tmpbuf[RUSS_REQ_SPATH_MAX];
	char		*bp = NULL, *bend = NULL, *bp2 = NULL;
	char		*sfmt = NULL, *lfmt = NULL;
	char		*services_dir = NULL;
	int		sdlen, cnt, stval;
	int		changed;
	int		n, nfollow;

	if ((spath == NULL) || (strncpy(buf, spath, sizeof(buf)) < 0)) {
		return NULL;
	}
	services_dir = russ_get_services_dir();
	sdlen = strlen(services_dir);
	bend = buf+sizeof(buf);

	/*
	* TODO: the following code could be simplified and
	* clarified so that flow is obvious.
	*/
	nfollow = 0;
	changed = 1;
	while (changed) {
		changed = 0;
		if (buf[0] != '\0') {
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
					} else if (follow && S_ISLNK(st.st_mode)) {
						if (++nfollow > RUSS_SPATH_RESOLVE_SYMLINKS_MAX) {
							return NULL;
						}
						if (readlink(buf, lnkbuf, sizeof(lnkbuf)) < 0) {
							/* insufficient space */
							return NULL;
						}
						lnkbuf[st.st_size] = '\0';

						if (lnkbuf[0] == '/') {
							/* replace subpath with lnkbuf */
							if (russ_snprintf(tmpbuf, sizeof(tmpbuf), "%s", lnkbuf) < 0) {
								return NULL;
							}
						} else {
							if ((bp2 = rindex(buf, '/')) != NULL) {
								/* append lnkbuf to subpath */
								*bp2 = '\0';
								if (russ_snprintf(tmpbuf, sizeof(tmpbuf), "%s/%s", buf, lnkbuf) < 0) {
									return NULL;
								}
								*bp2 = '/';
							} else {
								/* replace single component subpath with lnkbuf */
								if (russ_snprintf(tmpbuf, sizeof(tmpbuf), "%s", lnkbuf) < 0) {
									return NULL;
								}
							}
						}
						if (bp != NULL) {
							/* append path right of subpath */
							*bp = '/';
							strncat(tmpbuf, bp, sizeof(tmpbuf));
						}
						/* copy back to buf */
						if (russ_snprintf(buf, sizeof(buf), "%s", tmpbuf) < 0) {
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
* Simple case for russ_spath_resolvewithuid without current uid.
*/
char *
russ_spath_resolve(const char *spath) {
	uid_t	uid;
	uid = getuid();
	return russ_spath_resolvewithuid(spath, &uid, 1);
}

/**
* Find socket address from service path.
*
* @param spath	service path
* @return	socket address; NULL on failure
*/
char *
russ_find_socket_addr(const char *spath) {
	struct stat	st;
	char		*saddr = NULL;

	if ((spath = russ_spath_resolve(spath)) != NULL) {
		saddr = strdup(spath);
		while (stat(saddr, &st) != 0) {
			saddr = dirname(saddr);
		}
		if (S_ISSOCK(st.st_mode)) {
			spath = russ_free((char *)spath);
			return saddr;
		}
		spath = russ_free((char *)spath);
		saddr = russ_free(saddr);
	}
	return NULL;
}

char *
russ_get_plusserver_path(void) {
	return strdup(RUSS_SERVICES_DIR"/plus");
}

/**
* Split spath into saddr and remaing spath.
*
* A service path may be composed of a socket address (saddr) and a
* (remaining) service path (spath). The saddr is local, the spath is
* passed to the service server. Depending on the service server, the
* spath may also be a service target and need to be resolved and
* followed.
*
* Special handling: "/+", "+". Stops searching for saddr.
*
* @param spath		service path
* @param[out] saddr	socket address
* @param[out] spath2	remaining service path
* @return		0 on succes; -1 on failure
*/
int
russ_spath_split(const char *spath, char **saddr, char **spath2) {
	struct russ_target	*targ = NULL;
	struct stat		st;
	char			*p = NULL;
	char			_spath2[RUSS_REQ_SPATH_MAX];

	/* initialize */
	*saddr = NULL;
	*spath2 = NULL;

	/* must be non-NULL, non-empty string; return value must be free */
	if (((spath = russ_spath_resolve(spath)) == NULL)
		|| (spath[0] == '\0')) {
		goto free_spath;
	}

	/* special case */
	if (spath[0] == '+') {
		p = (char *)spath+1;
	} else if (strncmp(spath, "/+", 2) == 0) {
		p = (char *)spath+2;
	}
	if ((p) && ((p[0] == '/') || (p[0] == '\0'))) {
		*saddr = russ_get_plusserver_path();
		if (p[0] == '\0') {
			/* ensure minimal spath of "/" */
			p = "/";
		}
		*spath2 = strdup(p);
		return 0;
	}

	/*
	* search for socket file in spath as sub-paths (from left to
	* right); temporarily replacing '/' with '\0' avoids
	* needless copying
	*/
	p = (char *)spath;
	while (p != NULL) {
		if ((p = index(p+1, '/')) != NULL) {
			*p = '\0';
		}
		if (lstat(spath, &st) == 0) {
			if ((S_ISSOCK(st.st_mode)) || (S_ISREG(st.st_mode))) {
				/* found socket; position p to end or next char */
				if (p == NULL) {
					p = "\0";
				} else {
					p++;
				}
				break;
			} else if (!S_ISDIR(st.st_mode)) {
				/* non-socket file */
				goto free_spath;
			}
		}
		/* restore '/' if possible */
		if (p != NULL) {
			*p = '/';
		}
	}

	if (p == NULL) {
		/* no socket file */
		goto free_spath;
	}

	/* copy into target */
	if (((*saddr = strdup(spath)) == NULL)
		|| ((*spath2 = russ_malloc(strlen(p)+1+1)) == NULL)
		|| (snprintf(*spath2, RUSS_REQ_SPATH_MAX, "/%s", p) < 0)) {
		goto free_saddr;
	}
	spath = russ_free((char *)spath);
	return 0;

free_saddr:
	*saddr = russ_free(*saddr);
	*spath2 = russ_free(*spath2);
free_spath:
	spath = russ_free((char *)spath);
	return -1;
}

/**
* Return copy of service path without options.
*
* @param spath		service path
* @return		string (free by caller); NULL otherwise
*/
char *
russ_spath_stripoptions(const char *spath) {
	char		tmp[RUSS_REQ_SPATH_MAX];
	char		*dst;
	const char	*src;

	if (strlen(spath) > sizeof(tmp)) {
		return NULL;
	}
	dst = tmp;
	for (src = spath; *src != '\0'; src++, dst++) {
		if (*src == '?') {
			for (src++; (*src != '\0') && (*src != '/'); src++);
			if (*src == '\0') {
				break;
			}
		}
		*dst = *src;
	}
	*dst = '\0';
	return strdup(tmp);
}
