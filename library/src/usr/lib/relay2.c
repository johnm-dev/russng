/*
** relay2.c
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

/**
* Initialize existing relay2stream.
*
* Existing buffer is freed and a new one allocated.
*
* @param rfd		read fd
* @param wfd		write fd
* @param bufsize	size of buffer (to allocate)
* @param auto_close	1 to close fds
* @return		0 on success; -1 on failure
*/
int
russ_relay2stream_init(struct russ_relay2stream *self, int rfd, int wfd, int bufsize, int auto_close) {
	struct russ_buf	*rbuf;

	if ((self == NULL)
		|| ((rbuf = russ_buf_new(bufsize)) == NULL)) {
		return -1;
	}
	if (self->rbuf) {
		self->rbuf = russ_buf_free(self->rbuf);
	}
	self->rbuf = rbuf;
	self->rfd = rfd;
	self->wfd = wfd;
	self->auto_close = auto_close;
	self->bidir = 0;
	return 0;
}

/**
* Free relay2stream.
*
* @param self		relay2stream object
* @return		NULL;
*/
struct russ_relay2stream *
russ_relay2stream_free(struct russ_relay2stream *self) {
	if (self) {
		self->rbuf = russ_buf_free(self->rbuf);
		free(self);
	}
	return NULL;
}

/**
* Create relay2stream.
*
* @param rfd		read fd
* @param wfd		write fd
* @param bufsize	buffer size
* @param auto_close	flag to automatically close
* @return		relay2stream on success; NULL on failure
*/
struct russ_relay2stream *
russ_relay2stream_new(int rfd, int wfd, int bufsize, int auto_close) {
	struct russ_relay2stream	*self;

	if (((self = malloc(sizeof(struct russ_relay2stream))) == NULL)
		|| ((self->rbuf = russ_buf_new(bufsize)) == NULL)) {
		free(self);
		return NULL;
	}
	self->rfd = rfd;
	self->wfd = wfd;
	self->auto_close = auto_close;
	self->bidir = 0;
	return self;
}

/**
* Free relay2 object.
*
* @param self		relay2 object
* @return		NULL
*/
struct russ_relay2 *
russ_relay2_free(struct russ_relay2 *self) {
	int	i;

	if (self) {
		free(self->pollfds);
		for (i = 0; i < self->nstreams; i++) {
			self->streams[i] = russ_relay2stream_free(self->streams[i]);
		}
		free(self->streams);
		free(self);
	}
	return NULL;
}

/**
* Create new relay2 object.
*
* @param n		number of relay2streams to support
* @return		relay2 object; NULL on failure
*/
struct russ_relay2 *
russ_relay2_new(int n) {
	struct russ_relay2	*self;
	int			i;

	if ((self = malloc(sizeof(struct russ_relay2))) == NULL) {
		return NULL;
	}

	self->nstreams = n;
	self->exit_fd = -1;
	self->pollfds = NULL;

	if (((self->streams = malloc(sizeof(struct russ_relay2stream *)*n)) == NULL)
		|| ((self->pollfds = malloc(sizeof(struct pollfd)*(n+1))) == NULL)) {
		goto free_relay;
	}
	for (i = 0; i < n; i++) {
		self->streams[i] = NULL;
		self->pollfds[i].fd = -1;
		self->pollfds[i].events = 0;
	}
	/* last element (for exit_fd) */
	self->pollfds[i].fd = -1;
	self->pollfds[i].events = 0;

	return self;

free_relay:
	russ_relay2_free(self);
	return NULL;
}

/**
* Register an fd pair to relay (rfd -> wfd).
*
* @param self		relay object
* @param rfd		read fd
* @param wfd		write fd
* @param bufsize	relay buffer size
* @param auto_close	auto close flag
* @return		index; -1 on error
*/
int
russ_relay2_add(struct russ_relay2 *self, int rfd, int wfd, int bufsize, int auto_close) {
	int	i;

	for (i = 0; (i < self->nstreams) && (self->streams[i] != NULL); i++);
	if (i == self->nstreams) {
		return -1;
	}
	if ((self->streams[i] = russ_relay2stream_new(rfd, wfd, bufsize, auto_close)) == NULL) {
		return -1;
	}
	self->pollfds[i].fd = rfd;
	self->pollfds[i].events = POLLIN;

	return i;
}

/**
* Helper to russ_relay2_add() to add bidirectional relay.
*
* @see russ_relay2_add()
*
* @param self		relay object
* @param fd0		an fd
* @param fd1		another fd
* @param bufsize	relay buffer size
* @param auto_close	auto close flag
* @return		0 on success; -1 or error
*/
int
russ_relay2_add2(struct russ_relay2 *self, int fd0, int fd1, int bufsize, int auto_close) {
	int	i, j;

	if (((i = russ_relay2_add(self, fd0, fd1, bufsize, auto_close)) < 0)
		|| ((j = russ_relay2_add(self, fd1, fd0, bufsize, auto_close)) < 0)) {
		russ_relay2_remove(self, fd0, fd1);
		return -1;
	}
	self->streams[i]->bidir = 1;
	self->streams[j]->bidir = 1;
	return 0;
}

/**
* Find relay2stream and return index.
*
* @param self		relay object
* @param rfd		read fd
* @param wfd		write fd
* @return		index of relay2stream; -1 on failure
*/
int
russ_relay2_find(struct russ_relay2 *self, int rfd, int wfd) {
	int	i;

	for (i = 0; i < self->nstreams; i++) {
		if ((self->streams[i]) && (self->streams[i]->rfd == rfd) && (self->streams[i]->wfd == wfd)) {
			return i;
		}
	}
	return -1;
}

/**
* Remove fd pair.
*
* @param self		relay2 object
* @param rfd		read fd
* @param wfd		write fd
* @return		0 on success; -1 on error
*/
int
russ_relay2_remove(struct russ_relay2 *self, int rfd, int wfd) {
	int	i, j;

	if ((i = russ_relay2_find(self, rfd, wfd)) < 0) {
		return -1;
	}
	close(rfd);
	close(wfd);
	self->streams[i] = russ_relay2stream_free(self->streams[i]);
	self->pollfds[i].fd = -1;
	self->pollfds[i].events = 0;
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
russ_relay2_poll(struct russ_relay2 *self, int timeout) {
	return poll(self->pollfds, self->nstreams+1, timeout);
}

/**
* Relay data between registered fds.
*
* @param self		relay object
* @param timeout	time (ms) to serve
* @retrun		0 on success; -1 on error
*/
int
russ_relay2_serve(struct russ_relay2 *self, int timeout, int exit_fd) {
	struct pollfd		*pollfds, *pollfd;
	struct russ_buf		*rbuf;
	struct russ_relay2stream	*stream, **streams;
	int			events, nevents, revents;
	int			exited, fd;
	int			nactive, nstreams;
	int			i, cnt;

	exited = 0;
	pollfds = self->pollfds;
	streams = self->streams;
	nactive = self->nstreams+1;
	nstreams = self->nstreams;

	pollfds[nstreams].fd = exit_fd;
	pollfds[nstreams].events = POLLIN;

	while (nactive) {
//usleep(500000);
//usleep(100000);
		if ((nevents = russ_relay2_poll(self, timeout)) < 1) {
			break;
		}
		if (nevents == 0) {
			continue;
		}

		for (i = 0; nevents && (i < nstreams); i++) {
			if ((pollfds[i].fd < 0) || (pollfds[i].revents == 0)) {
				/* disabled, uneventful */
				continue;
			}

			/* set aliases */
			pollfd = &pollfds[i];
			fd = pollfd->fd;
			events = pollfd->events;
			revents = pollfd->revents;
			stream = streams[i];

			if (revents & POLLIN) {
				rbuf = stream->rbuf;
				if ((cnt = russ_read(fd, rbuf->data, rbuf->cap)) <= 0) {
					/* EOF or error; unrecoverable */
					goto disable_stream;
				}
				rbuf->len = cnt;
				rbuf->off = 0;

				pollfd->fd = stream->wfd;
				pollfd->events = POLLOUT;
			} else if (revents & POLLOUT) {
				rbuf = stream->rbuf;
				if ((cnt = russ_write(fd, rbuf->data+rbuf->off, rbuf->len)) < 0) {
					/* error; unrecoverable */
					goto disable_stream;
				}
				rbuf->off += cnt;
				if (rbuf->off == rbuf->len) {
					rbuf->off = 0;
					rbuf->len = 0;

					if (exited) {
						goto disable_stream;
					}
					pollfd->fd = stream->rfd;
					pollfd->events = POLLIN;
				}
			} else if (revents & POLLHEN) {
disable_stream:
				russ_relay2_remove(self, stream->rfd, stream->wfd);
				nactive--;
			}
			nevents--;
		}

		/* special case exit_fd */
		if ((pollfds[nstreams].fd == exit_fd) && (pollfds[nstreams].revents & POLLHUP)) {
			if (nactive == 1) {
				break;
			}
			if (!exited) {
				exited = 1;
				nactive--;
			}
			for (i = 0; i < nstreams; i++) {
				if ((pollfds[i].fd >= 0) && (pollfds[i].events & POLLIN)) {
					russ_relay2_remove(self, streams[i]->rfd, streams[i]->wfd);
					nactive--;
				}
			}
		}
	}
}