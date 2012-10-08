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

#include <grp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
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
* Get current time as deadline value.
*
* @return		deadline value
*/
inline russ_timeout
russ_gettime(void) {
	struct timespec	tp;

	clock_gettime(CLOCK_MONOTONIC, &tp);
	return RUSS_DEADLINE_BIT | (tp.tv_sec*1000 + (tp.tv_nsec/1000000));
}

/**
* Convert deadline value to timeout, relative to the current time.
*
* @param value		deadline value
* @return		computed timeout value; value if already a timeout
*/
inline russ_timeout
russ_to_timeout(russ_timeout value) {
	if (value & RUSS_DEADLINE_BIT) {
		value -= russ_gettime();
	}
	return value;
}

/**
* Convert deadline value to timeout (with floor of 0), relative to
* the current time.
*
* @param value		deadline value
* @return		computed timeout value with floor of 0
*/
inline russ_timeout
russ_to_timeout_future(russ_timeout value) {
	if ((value = russ_to_timeout(value)) < 0) {
		value = 0;
	}
	return value;
}

/**
* Convert timeout value to deadline, relative to the current time.
*
* @param value		timeout value
* @return		computed deadline value; value if already a deadline
*/
inline russ_timeout
russ_to_deadline(russ_timeout value) {
	if (value & RUSS_DEADLINE_BIT) {
		return value;
	}
	return russ_gettime()+value;
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
	if ((saddr = russ_resolve_addr(saddr)) == NULL) {
		return -1;
	}
	if (unlink(saddr) < 0) {
		free(saddr);
		return -1;
	}
	free(saddr);
	return 0;
}
