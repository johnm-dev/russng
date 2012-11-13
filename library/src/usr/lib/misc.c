/*
** lib/misc.c
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

#include <grp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Count elements of NULL-terminated string array (not including
* NULL).
*
* @param arr		string array
* @param max_cnt	maximum # of items to look for
* @return		# of strings upto NULL; -1 if arr == NULL; max_cnt if reached
*/
int
russ_sarray0_count(char **arr, int max_cnt) {
	int	i;

	if (arr == NULL) {
		return -1;
	}
	for (i = 0; (i < max_cnt) && (arr[i] != NULL); i++);
	return i;
}

/**
* Duplicate a NULL-terminated string array.
*
* @param src		source string array
* @param max_cnt	max # of elements supported
* @return		duplicated array
*/
char **
russ_sarray0_dup(char **src, int max_cnt) {
	char	**dst;
	int	i, cnt;

	if (((cnt = russ_sarray0_count(src, max_cnt)) < 0)
		|| (cnt == max_cnt)) {
		return NULL;
	}
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
	return dst;
free_dst:
	for (; i >= 0; i--) {
		free(dst[i]);
	}
	return NULL;
}

/**
* Free NULL-terminated string array.
*
* @param arr		NULL-terminated string array
*/
void
russ_sarray0_free(char **arr) {
	char	**p;

	if (arr) {
		for (p = arr; *p != NULL; p++) {
			free(*p);
		}
		free(arr);
	}
}

/**
* fprintf-like for descriptor instead of FILE *.
*
* @param fd		descriptor
* @param format		printf-style format string
* @param ...		variadic list of arguments
* @return		# of bytes written; -1 on error
*/
int
russ_dprintf(int fd, char *format, ...) {
	char	buf[4096];
	int	n;
	va_list	ap;

	/* TODO: use realloc to handle any size output */
	va_start(ap, format);
	n = vsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	if (n >= 0) {
		if (russ_writen(fd, buf, n) < n) {
			return -1;
		}
	}
	return n;
}

/**
* Switch user (uid, gid, supplemental groups).
*
* This will succeed for non-root trying to setuid/setgid to own
* credentials (a noop and gids is ignored). As root, this should
* always succeed.
*
* Supplemental groups require attention so that root supplemental
* group entry of 0 does not get carried over. No supplemental
* group information is set up (only erased).
*
* @param uid		user id
* @param gid		group id
* @param ngids		number of supplemental groups in list
* @param gids		list of supplemental gids
* @return		0 on success; -1 on failure
*/
int
russ_switch_user(uid_t uid, gid_t gid, int ngids, gid_t *gids) {
	gid_t	_gid, *_gids;
	int	_ngids;

	if ((uid == getuid()) && (gid == getgid())) {
	    return 0;
	}

	/* save settings */
	_gid = getgid();
	_ngids = 0;
	_gids = NULL;
	if (((_ngids = getgroups(0, NULL)) < 0)
		|| ((_gids = malloc(sizeof(gid_t)*_ngids)) == NULL)
		|| (getgroups(_ngids, _gids) < 0)) {
		return -1;
	}

	if ((setgroups(ngids, gids) < 0)
		|| (setgid(gid) < 0)
		|| (setuid(uid) < 0)) {
		goto restore;
	}
	free(_gids);
	return 0;
restore:
	/* restore setting */
	setgroups(_ngids, _gids);
	free(_gids);
	setgid(_gid);
	/* no need to restore uid */
	return -1;
}

/**
* Unlink/remove an existing socket file.
*
* Resolves the address and unlinks the file.
*
* @param saddr		socket address
* @return		0 on success; -1 on failure
*/
int
russ_unlink(char *saddr) {
	if ((saddr = russ_resolve_spath(saddr)) == NULL) {
		return -1;
	}
	if (unlink(saddr) < 0) {
		free(saddr);
		return -1;
	}
	free(saddr);
	return 0;
}
