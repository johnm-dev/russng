/*
* lib/user.c
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
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include <russ.h>

/**
* Convert group as gid or groupname string into a gid.
*
* @param group		gid/groupname string
* @return		gid; -1 on failure
*/
gid_t
russ_group2gid(char *group) {
	struct group	*gr;
	gid_t	gid;

	if ((group) && ((group[0] >= '0') && (group[0] <= '9'))) {
		if (sscanf(group, "%d", &gid) < 1) {
			gid = -1;
		}
	} else {
		gid = ((gr = getgrnam(group)) == NULL) ? -1 : gr->gr_gid;
	}
	return (gid >= 0) ? gid : -1;
}

/**
* Base switch user (uid, gid, supplemental groups).
*
* If doinitgroups is non-zero, initgroups() is called to set the
* supplemental groups apart from the gids provided. Otherwise, the
* gids provided (if any) are used.
*
* @param uid		user id
* @param gid		group id
* @param ngids		number of supplemental groups in list
* @param gids		list of supplemental gids
* @param doinitgroups	flag to use (non-zero) initgroups() to set
*			supplemental groups
* @return		0 on success; -1 on failure
*/
static int
_russ_switch_user(uid_t uid, gid_t gid, int ngids, gid_t *gids, int doinitgroups) {
	struct passwd	*pw;
	uid_t		_uid;
	gid_t		_gid, *_gids;
	int		_ngids;

	_uid = getuid();
	_gid = getgid();

	if ((uid == _uid) && (gid == _gid)) {
		return 0;
	}

	/* save settings */
	_gid = getgid();
	_ngids = 0;
	_gids = NULL;
	if (((_ngids = getgroups(0, NULL)) < 0)
		|| ((_gids = russ_malloc(sizeof(gid_t)*_ngids)) == NULL)
		|| (getgroups(_ngids, _gids) < 0)) {
		if (_gids) {
			_gids = russ_free(_gids);
		}
		return -1;
	}

	if (doinitgroups) {
		if (((pw = getpwuid(uid)) == NULL)
			|| (initgroups(pw->pw_name, gid) < 0)
			|| (setgid(gid) < 0)
			|| (setuid(uid) < 0)) {
			goto restore;
		}
	} else {
		if ((setgroups(ngids, gids) < 0)
			|| (setgid(gid) < 0)
			|| (setuid(uid) < 0)) {
			goto restore;
		}
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
* @see russ_switch_userinitgroups().
*
* @param uid		user id
* @param gid		group id
* @param ngids		number of supplemental groups in list
* @param gids		list of supplemental gids
* @return		0 on success; -1 on failure
*/
int
russ_switch_user(uid_t uid, gid_t gid, int ngids, gid_t *gids) {
	return _russ_switch_user(uid, gid, ngids, gids, 0);
}

/**
* Switch user (uid, gid) and initialize supplemental groups.
*
* Like russ_switch_user() but with supplemental groups obtained from
* the system.
*
* @param uid		user id
* @param gid		group id
* @return		0 on success; -1 on failure
*/
int
russ_switch_userinitgroups(uid_t uid, gid_t gid) {
	return _russ_switch_user(uid, gid, 0, NULL, 1);
}

/**
* Convert user as uid or username string into a uid.
*
* @param group		uid/username string
* @return		uid; -1 on failure
*/
uid_t
russ_user2uid(char *user) {
	struct passwd	*pw;
	uid_t		uid;

	if ((user) && ((user[0] >= '0') && (user[0] <= '9'))) {
		if (sscanf(user, "%d", &uid) < 1) {
			uid = -1;
		}
	} else {
		uid = ((pw = getpwnam(user)) == NULL) ? -1 : pw->pw_uid;
	}
	return (uid >= 0) ? uid : -1;
}
