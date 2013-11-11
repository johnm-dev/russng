/*
** relay.c
*/

/*
# license--start
#
# Copyright 2013 John Marshall
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

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>

#include <russ_priv.h>

#define POLLHEN	(POLLHUP|POLLERR|POLLNVAL)

static struct russ_relaydata *
russ_relaydata_free(struct russ_relaydata *self) {
	if (self) {
		russ_buf_free(self->rbuf);
		free(self);
	}
	return NULL;
}

static struct russ_relaydata *
russ_relaydata_new(int fd, int bufsize, int auto_close) {
	struct russ_relaydata	*self;

	if ((self = malloc(sizeof(struct russ_relaydata))) == NULL) {
		return NULL;
	}
	if ((self->rbuf = russ_buf_new(bufsize)) == NULL) {
		goto free_relaydata;
	}
	self->fd = fd;
	self->auto_close = auto_close;
	return self;

free_relaydata:
	russ_relaydata_free(self);
	return NULL;
}

static void
russ_relaydata_init(struct russ_relaydata *self, int fd, struct russ_buf *rbuf, int auto_close) {
	self->fd = fd;
	self->rbuf = rbuf;
	self->auto_close = auto_close;
}

/**
* Create new relay object.
*
* @param n		number of fds to serve
* @return		relay object; NULL on failure
*/
struct russ_relay *
russ_relay_new(int n) {
	struct russ_relay	*self;
	int			i;

	if ((self = malloc(sizeof(struct russ_relay))) == NULL) {
		return NULL;
	}
	self->nfds = n;
	self->pollfds = NULL;
	self->rdatas = NULL;

	if (((self->pollfds = malloc(sizeof(struct pollfd)*n)) == NULL)
		|| ((self->rdatas = malloc(sizeof(struct russ_relaydata *)*n)) == NULL)) {
		goto free_relay;
	}

	for (i = 0; i < n; i++) {
		self->pollfds[i].fd = -1;
		self->pollfds[i].events = 0;
		self->rdatas[i] = NULL;
	}

	return self;

free_relay:
	russ_relay_free(self);
	return NULL;
}

/**
* Free relay object.
*
* @param self		relay object
* @return		NULL
*/
struct russ_relay *
russ_relay_free(struct russ_relay *self) {
	int	i;

	if (self) {
		free(self->pollfds);
		for (i = 0; i < self->nfds; i++) {
			russ_relaydata_free(self->rdatas[i]);
		}
		free(self->rdatas);
		free(self);
	}
	return NULL;
}

/**
* Register an fd pair for bidirectional (fd<->fd) data relay.
*
* @param self		relay object
* @param dir		relay direction
* @param fda		an fd
* @param bufsizea	outgoing buffer size for fda
* @param auto_closea	auto close flag for fda
* @param fdb		an fd
* @param bufsizeb	outgoing buffer size for fdb
* @param auto_closeb	auto close flag for fdb
* @return		0 on success; -1 on error
*/
int
russ_relay_add(struct russ_relay *self, int dir,
	int fd0, int bufsize0, int auto_close0,
	int fd1, int bufsize1, int auto_close1) {
	struct russ_relaydata	*rdatas[2] = {NULL, NULL};
	int			i;

	if (((rdatas[0] = russ_relaydata_new(fd0, bufsize0, auto_close0)) == NULL)
		|| ((rdatas[1] = russ_relaydata_new(fd1, bufsize1, auto_close1)) == NULL)) {
		goto free_rdatas;
	}

	for (i = 0; i < self->nfds; i += 2) {
		if (self->rdatas[i] == NULL) {
			self->pollfds[i].fd = fd0;
			self->pollfds[i].events = (dir & RUSS_RELAYDIR_WE) ? POLLIN : 0;
			self->rdatas[i] = rdatas[0];

			self->pollfds[i+1].fd = fd1;
			self->pollfds[i+1].events = (dir & RUSS_RELAYDIR_EW) ? POLLIN : 0;
			self->rdatas[i+1] = rdatas[1];
			break;
		}
	}
	if (i == self->nfds) {
		goto free_rdatas;
	}
	return 0;

free_rdatas:
	russ_relaydata_free(rdatas[0]);
	russ_relaydata_free(rdatas[1]);
	return -1;
}

/**
* Remove fd.
*
* @param self		relay object
* @param fd		an fd
* @return		0 on success; -1 on error
*/
int
russ_relay_remove(struct russ_relay *self, int fd) {
	int	i;

	for (i = 0; i < self->nfds; i++) {
		if ((self->rdatas[i]) && (self->rdatas[i]->fd == fd)) {
			if (self->rdatas[i]->auto_close) {
				russ_close(self->rdatas[i]->fd);
			}
			self->pollfds[i].fd = -1;
			self->rdatas[i] = russ_relaydata_free(self->rdatas[i]);
			break;
		}
	}
	if (i == self->nfds) {
		return -1;
	}
	return 0;
}

/**
* Poll for registered fds.
*
* @param self		relay object
* @param timeout	time (ms) to poll
* @return		number of fds with events; -1 on error
*/
int
russ_relay_poll(struct russ_relay *self, int timeout) {
	return poll(self->pollfds, self->nfds, timeout);
}

/**
* Serve I/O for registered fds.
*
* @param self		relay object
* @param timeout	time (ms) to serve
* @retrun		0 on success; -1 on error
*/
int
russ_relay_serve(struct russ_relay *self, int timeout) {
	struct pollfd		*pollfds, *pollfd, *opollfd;
	struct russ_relaydata	**rdatas;
	struct russ_buf		*rbuf, *orbuf;
	int			oidx;
	int			fd, ofd;
	int			events, revents;
	int			cnt, nevents, nfds;
	int			i;

	nfds = self->nfds;
	pollfds = self->pollfds;
	rdatas = self->rdatas;

	while (nfds) {
//usleep(500000);
//usleep(100000);

		if ((nevents = russ_relay_poll(self, timeout)) < 1) {
			break;
		}
		if (nevents == 0) {
			continue;
		}

		for (i = 0; nevents && (i < self->nfds); i++) {
			if ((pollfds[i].fd < 0) || (pollfds[i].revents == 0)) {
				continue;
			}

			/* set aliases */
			pollfd = &pollfds[i];
			fd = pollfd->fd;
			events = pollfd->events;
			revents = pollfd->revents;

			oidx = (i & 0x1) ? i-1 : i+1;
			ofd = rdatas[oidx]->fd;
			opollfd = &pollfds[oidx];			

			if (revents & POLLIN) {
				orbuf = rdatas[oidx]->rbuf;
				if ((cnt = russ_read(fd, orbuf->data, orbuf->cap)) <= 0) {
					/* EOF or error; unrecoverable */
					goto disable_relay;
				}
				orbuf->len = cnt;
				orbuf->off = 0;

				/* flip */
				pollfd->events &= ~POLLIN;
				opollfd->events |= POLLOUT;
				opollfd->fd = ofd;
				if (pollfds[oidx].revents & POLLHEN) {
					/* altered events; jump to next fd pair */
					i = ((i/2)+1)*2;
				}
				//break;
			} else if (revents & POLLOUT) {
				rbuf = rdatas[i]->rbuf;
				if ((cnt = russ_write(fd, rbuf->data+rbuf->off, rbuf->len)) < 0) {
					/* error; unrecoverable */
					goto disable_relay;
				}
				rbuf->off += cnt;
				if (rbuf->off == rbuf->len) {
					/* buffer written, reset */
					rbuf->off = 0;
					rbuf->len = 0;

					/* flip */
					pollfd->events &= ~POLLOUT;
					opollfd->events |= POLLIN;
					opollfd->fd = ofd;
				}
				if (pollfds[oidx].revents & POLLHEN) {
					/* altered events; jump to next fd pair */
					i = ((i/2)+1)*2;
				}
				//break;
			} else if (revents & POLLHEN) {
				if (!(opollfd->events & POLLOUT)) {
disable_relay:
					russ_relay_remove(self, fd);
					russ_relay_remove(self, ofd);
					nfds -= 2;
					break;
				}
			}
			nevents--;
		}
	}
}