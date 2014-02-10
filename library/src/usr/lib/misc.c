/*
* lib/misc.c
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
	char	_buf[8192];
	char	*buf;
	int	n, bufsz;

	buf = _buf;
	bufsz = sizeof(_buf);
	while (1) {
		va_start(ap, format);
		n = vsnprintf(buf, bufsz, format, ap);
		va_end(ap);
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

/**
* Free memory _and_ return NULL.
*
* This consolidates into one call the good practice of resetting
* a pointer to NULL after free().
*
* @param p		pointer to malloc'd memory
* @return		NULL
*/
void *
russ_free(void *p) {
	free(p);
	return NULL;
}

/**
* Wrapper for malloc to support 0-sized malloc requests
* (see AIX malloc()).
*
* @param size		number of bytes
* @return		pointer to allocated memory
*/
void *
russ_malloc(size_t size) {
	size = (size == 0) ? 1 : size;
	return malloc(size);
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
	_gids = russ_free(_gids);
	return 0;
restore:
	/* restore setting */
	setgroups(_ngids, _gids);
	_gids = russ_free(_gids);
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
russ_unlink(const char *saddr) {
	if ((saddr = russ_spath_resolve(saddr)) == NULL) {
		return -1;
	}
	if (unlink(saddr) < 0) {
		saddr = russ_free((char *)saddr);
		return -1;
	}
	saddr = russ_free((char *)saddr);
	return 0;
}
