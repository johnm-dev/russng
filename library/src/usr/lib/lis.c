/*
* lib/lis.c
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

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Create a russ_lis object.
*
* @param sd		initial descriptor
* @return		russ_lis object; NULL on failure
*/
struct russ_lis *
russ_lis_new(int sd) {
	struct russ_lis	*self;

	if ((self = malloc(sizeof(struct russ_lis))) == NULL) {
		return NULL;
	}
	self->sd = sd;
	return self;
}

/**
* Announce service as a socket file.
*
* If the address already exists (EADDRINUSE), then we check to see
* if anything is actually using it. If not, we remove it and try to
* set it up. If the address cannot be "bind"ed, then we exit with
* NULL.
*
* The only way to claim an address that is in use it to forcibly
* remove it from the filesystem first (unlink), then call here.
*
* @param saddr		socket address
* @param mode		file mode of path
* @param uid		owner of path
* @param gid		group owner of path
* @return		listener socket descriptor
*/
int
russ_announce(char *saddr, mode_t mode, uid_t uid, gid_t gid) {
	struct sockaddr_un	servaddr;
	int			lisd;

	if ((saddr == NULL) || ((saddr = russ_spath_resolve(saddr)) == NULL)) {
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, saddr);
	if ((lisd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		goto free_saddr;
	}
	if (bind(lisd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		if ((errno == EADDRINUSE)
			&& (connect(lisd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)) {
			/* is something listening? */
			if (errno != ECONNREFUSED) {
				goto close_lisd;
			} else if ((unlink(saddr) < 0)
				|| (bind(lisd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)) {
				goto close_lisd;
			}
		} else {
			goto close_lisd;
		}
	}
	if ((chmod(saddr, mode) < 0)
		|| (chown(saddr, uid, gid) < 0)
		|| (listen(lisd, RUSS_LISTEN_BACKLOG) < 0)) {
		goto close_lisd;
	}
	saddr = russ_free(saddr);
	return lisd;

close_lisd:
	russ_close(lisd);
free_saddr:
	saddr = russ_free(saddr);
	return -1;
}

/**
* Close listener.
*
* @param self		listener object
*/
void
russ_lis_close(struct russ_lis *self) {
	if (self->sd > -1) {
		russ_fds_close(&self->sd, 1);
	}
}

/**
* Free listener object.
*
* @param self		listener object
* @return		NULL value
*/
struct russ_lis *
russ_lis_free(struct russ_lis *self) {
	self = russ_free(self);
	return NULL;
}
