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

#include <russ.h>
#include <russ_relay.h>

#define POLLHEN	(POLLHUP|POLLERR|POLLNVAL)

static void
russ_relaydata_init(struct russ_relaydata *self, int fd, char *buf, int bufsize, int auto_close) {
	self->fd = fd;
	self->buf = buf;
	self->bufsize = bufsize;
	self->bufcnt = 0;
	self->bufoff = 0;
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
	struct russ_relaydata	*data;
	int			i;

	if ((self = malloc(sizeof(struct russ_relay *))) == NULL) {
		return NULL;
	}
	self->nfds = n;
	self->pollfds = NULL;
	self->datas = NULL;

	if (((self->pollfds = malloc(sizeof(struct pollfd)*n)) == NULL)
		|| ((self->datas = malloc(sizeof(struct russ_relaydata)*n)) == NULL)) {
		goto free_relay;
	}

	for (i = 0; i < n; i++) {
		self->pollfds[i].fd = -1;
		self->pollfds[i].events = 0;
		russ_relaydata_init(&self->datas[i], -1, NULL, 0, 0);
	}

	return self;

free_relay:
	free(self->pollfds);
	free(self->datas);
	free(self);
	return NULL;
}

/**
* Destroy relay object.
*
* @param self		relay object
* @return		NULL
*/
struct russ_relay *
russ_relay_destroy(struct russ_relay *self) {
	if (self) {
		free(self->pollfds);
		free(self->datas);
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
	char	*bufs[2] = {NULL, NULL};
	int	idxs[2];
	int	i;

	if (((bufs[0] = malloc(bufsize0)) == NULL)
		|| ((bufs[1] = malloc(bufsize1)) == NULL)) {
		goto free_bufs;
	}

	for (i = 0; i < self->nfds; i += 2) {
		if (self->datas[i].fd == -1) {
			self->pollfds[i].fd = fd0;
			self->pollfds[i].events = (dir & RUSS_RELAYDIR_WE) ? POLLIN : 0;
			russ_relaydata_init(&self->datas[i], fd0, bufs[0], bufsize0, auto_close0);

			self->pollfds[i+1].fd = fd1;
			self->pollfds[i+1].events = (dir & RUSS_RELAYDIR_EW) ? POLLIN : 0;
			russ_relaydata_init(&self->datas[i+1], fd1, bufs[1], bufsize1, auto_close1);
			break;
		}
	}
	if (i == self->nfds) {
		goto free_bufs;
	}
	return 0;

free_bufs:
	free(bufs[0]);
	free(bufs[1]);
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
	int	idx;
	int	i;

	for (i = 0; i < self->nfds; i++) {
		if (self->datas[i].fd == fd) {
			if (self->datas[i].auto_close) {
				russ_close(self->datas[i].fd);
			}
			self->pollfds[i].fd = -1;
			free(self->datas[i].buf);
			russ_relaydata_init(&self->datas[i], -1, NULL, 0, 0);
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
	struct russ_relaydata	*datas, *data, *odata;
	char			*buf;
	int			idx, oidx;
	int			fd, events, revents;
	int			cnt, nevents, nfds, repoll;
	int			i;
	
	nfds = self->nfds;
	pollfds = self->pollfds;
	datas = self->datas;

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
			pollfd = &pollfds[i];
			fd = pollfd->fd;
			events = pollfd->events;
			revents = pollfd->revents;
			if ((fd < 0) || (revents == 0)) {
				continue;
			}

			oidx = (i & 0x1) ? i-1 : i+1;
			data = &datas[i];
			opollfd = &pollfds[oidx];
			odata = &datas[oidx];

			if (revents & POLLIN) {
				if ((cnt = russ_read(fd, odata->buf, odata->bufsize)) <= 0) {
					/* EOF or error; unrecoverable */
					goto disable_relay;
				}
				odata->bufcnt = cnt;
				odata->bufoff = 0;

				/* flip */
				pollfd->events &= ~POLLIN;
				opollfd->events |= POLLOUT;
				opollfd->fd = odata->fd;
				if (pollfds[oidx].revents & POLLHEN) {
					/* altered events; jump to next fd pair */
					i = ((i/2)+1)*2;
				}
				//break;
			} else if (revents & POLLOUT) {
				if ((cnt = russ_write(fd, data->buf+data->bufoff, data->bufcnt)) < 0) {
					/* error; unrecoverable */
					goto disable_relay;
				}
				data->bufoff += cnt;
				if (data->bufoff == data->bufcnt) {
					/* buffer written, reset */
					data->bufoff = 0;
					data->bufcnt = 0;

					/* flip */
					pollfd->events &= ~POLLOUT;
					opollfd->events |= POLLIN;
					opollfd->fd = odata->fd;
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
					russ_relay_remove(self, odata->fd);
					nfds -= 2;
					break;
				}
			}
			nevents--;
		}
	}
}