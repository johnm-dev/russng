/*
* lib/relay.c
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
* Initialize existing relaystream.
*
* Existing buffer is freed and a new one allocated.
*
* @param rfd		read fd
* @param wfd		write fd
* @param bufsize	size of buffer (to allocate)
* @param closeonexit	1 to close fds on exit (see russ_relay_serve())
* @return		0 on success; -1 on failure
*/
int
russ_relaystream_init(struct russ_relaystream *self, int rfd, int wfd, int bufsize, int closeonexit) {
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
	self->closeonexit = closeonexit;
	self->bidir = 0;

	/* stats */
	self->rlast = 0;
	self->wlast = 0;
	self->nrbytes;
	self->nwbytes;
	self->nreads = 0;
	self->nwrites = 0;

	return 0;
}

/**
* Free relaystream.
*
* @param self		relaystream object
* @return		NULL;
*/
struct russ_relaystream *
russ_relaystream_free(struct russ_relaystream *self) {
	if (self) {
		self->rbuf = russ_buf_free(self->rbuf);
		self = russ_free(self);
	}
	return NULL;
}

/**
* Create relaystream.
*
* @param rfd		read fd
* @param wfd		write fd
* @param bufsize	buffer size
* @param closeonexit	close on exit flag
* @return		relaystream on success; NULL on failure
*/
struct russ_relaystream *
russ_relaystream_new(int rfd, int wfd, int bufsize, int closeonexit, russ_relaystream_callback cb, void *cbarg) {
	struct russ_relaystream	*self;

	if ((rfd < 0) || (wfd < 0)) {
#if 0
		if (rfd >= 0) {
			close(rfd);
		}
		if (wfd >= 0) {
			close(wfd);
		}
#endif
		return NULL;
	}
	if (((self = malloc(sizeof(struct russ_relaystream))) == NULL)
		|| ((self->rbuf = russ_buf_new(bufsize)) == NULL)) {
		self = russ_free(self);
		return NULL;
	}

	self->rfd = rfd;
	self->wfd = wfd;
	self->closeonexit = closeonexit;
	self->bidir = 0;
	self->cb = cb;
	self->cbarg = cbarg;

	/* stats */
	self->rlast = 0;
	self->wlast = 0;
	self->nrbytes;
	self->nwbytes;
	self->nreads = 0;
	self->nwrites = 0;

	return self;
}

/**
* Read input into buffer.
*
* @param self		russ_relaystream object
* @return		number of bytes read; -1 on failure
*/
int
russ_relaystream_read(struct russ_relaystream *self) {
	struct russ_buf	*rbuf;
	int		cnt;

	rbuf = self->rbuf;
	if ((cnt = russ_read(self->rfd, rbuf->data, rbuf->cap)) > 0) {
		rbuf->len = cnt;
		rbuf->off = 0;
		self->rlast = russ_gettime();
		self->nrbytes += cnt;
		self->nreads++;

		if (self->cb) {
			self->cb(self, 0, self->cbarg);
		}
	}
	return cnt;
}

/**
* Write from buffer to output.
*
* @param self		russ_relaystream object
* @retrun		number of bytes written; -1 on failure
*/
int
russ_relaystream_write(struct russ_relaystream *self) {
	struct russ_buf	*rbuf;
	int		cnt;

	rbuf = self->rbuf;
	if ((cnt = russ_write(self->wfd, rbuf->data+rbuf->off, rbuf->len)) > 0) {
		rbuf->off += cnt;
		if (rbuf->off == rbuf->len) {
			rbuf->off = 0;
			rbuf->len = 0;
		}
		self->wlast = russ_gettime();
		self->nwbytes += cnt;
		self->nwrites++;

		if (self->cb) {
			self->cb(self, 1, self->cbarg);
		}
	}
	return cnt;
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
		self->pollfds = russ_free(self->pollfds);
		for (i = 0; i < self->nstreams; i++) {
			self->streams[i] = russ_relaystream_free(self->streams[i]);
		}
		self->streams = russ_free(self->streams);
		self = russ_free(self);
	}
	return NULL;
}

/**
* Create new relay object.
*
* @param n		number of relaystreams to support
* @return		relay object; NULL on failure
*/
struct russ_relay *
russ_relay_new(int n) {
	struct russ_relay	*self;
	int			i;

	if ((self = malloc(sizeof(struct russ_relay))) == NULL) {
		return NULL;
	}

	self->nstreams = n;
	self->exitfd = -1;
	self->pollfds = NULL;

	if (((self->streams = malloc(sizeof(struct russ_relaystream *)*n)) == NULL)
		|| ((self->pollfds = malloc(sizeof(struct pollfd)*(n+1))) == NULL)) {
		goto free_relay;
	}
	for (i = 0; i < n; i++) {
		self->streams[i] = NULL;
		self->pollfds[i].fd = -1;
		self->pollfds[i].events = 0;
	}
	/* last element (for exitfd) */
	self->pollfds[i].fd = -1;
	self->pollfds[i].events = 0;

	return self;

free_relay:
	russ_relay_free(self);
	return NULL;
}

/**
* Register an fd pair to relay (rfd -> wfd).
*
* @param self		relay object
* @param rfd		read fd
* @param wfd		write fd
* @param bufsize	relay buffer size
* @param closeonexit	close on exit flag
* @param cb		callback called for each I/O operation
* @param cbarg		callback argument object
* @return		index; -1 on error
*/
int
russ_relay_addwithcallback(struct russ_relay *self, int rfd, int wfd, int bufsize, int closeonexit,
	russ_relaystream_callback cb, void *cbarg) {
	int	i;

	for (i = 0; (i < self->nstreams) && (self->streams[i] != NULL); i++);
	if (i == self->nstreams) {
		return -1;
	}
	if ((self->streams[i] = russ_relaystream_new(rfd, wfd, bufsize, closeonexit, cb, cbarg)) == NULL) {
		return -1;
	}
	self->pollfds[i].fd = rfd;
	self->pollfds[i].events = POLLIN;

	return i;
}

/**
* Register an fd pair to relay (rfd -> wfd).
*
* Like russ_relay_addwithcallback() but with NULL callback and
* NULL callback arg.
*
* @param self		relay object
* @param rfd		read fd
* @param wfd		write fd
* @param bufsize	relay buffer size
* @param closeonexit	close on exit flag
* @return		index; -1 on error
*/
int
russ_relay_add(struct russ_relay *self, int rfd, int wfd, int bufsize, int closeonexit) {
	return russ_relay_addwithcallback(self, rfd, wfd, bufsize, closeonexit, NULL, NULL);
}

/**
* Helper to russ_relay_add() to add bidirectional relay.
*
* @see russ_relay_add()
*
* @param self		relay object
* @param fd0		an fd
* @param fd1		another fd
* @param bufsize	relay buffer size
* @param closeonexit	close on exit flag
* @return		0 on success; -1 or error
*/
int
russ_relay_add2(struct russ_relay *self, int fd0, int fd1, int bufsize, int closeonexit) {
	int	i, j;

	if (((i = russ_relay_add(self, fd0, fd1, bufsize, closeonexit)) < 0)
		|| ((j = russ_relay_add(self, fd1, fd0, bufsize, closeonexit)) < 0)) {
		russ_relay_remove(self, fd0, fd1);
		return -1;
	}
	self->streams[i]->bidir = 1;
	self->streams[j]->bidir = 1;
	return 0;
}

/**
* Find relaystream and return index.
*
* @param self		relay object
* @param rfd		read fd
* @param wfd		write fd
* @return		index of relaystream; -1 on failure
*/
int
russ_relay_find(struct russ_relay *self, int rfd, int wfd) {
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
* @param self		relay object
* @param rfd		read fd
* @param wfd		write fd
* @return		0 on success; -1 on error
*/
int
russ_relay_remove(struct russ_relay *self, int rfd, int wfd) {
	int	i, j;

	if ((i = russ_relay_find(self, rfd, wfd)) < 0) {
		return -1;
	}
	close(rfd);
	close(wfd);
	self->streams[i] = russ_relaystream_free(self->streams[i]);
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
russ_relay_poll(struct russ_relay *self, int timeout) {
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
russ_relay_serve(struct russ_relay *self, int timeout, int exitfd) {
	struct pollfd		*pollfds, *pollfd;
	struct russ_buf		*rbuf;
	struct russ_relaystream	*stream, **streams;
	int			events, nevents, revents;
	int			fd;
	int			nactive, nstreams;
	int			i, cnt;

	pollfds = self->pollfds;
	streams = self->streams;
	nactive = self->nstreams+1;
	nstreams = self->nstreams;

	pollfds[nstreams].fd = exitfd;
	pollfds[nstreams].events = POLLIN;

	while (nactive) {
//usleep(500000);
//usleep(100000);
		if ((nevents = russ_relay_poll(self, timeout)) < 1) {
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
				if (russ_relaystream_read(stream) <= 0) {
					/* EOF or error; unrecoverable */
					goto disable_stream;
				}
				pollfd->fd = stream->wfd;
				pollfd->events = POLLOUT;
			} else if (revents & POLLOUT) {
				if (russ_relaystream_write(stream) < 0) {
					/* error; unrecoverable */
					goto disable_stream;
				}
				if (stream->rbuf->len == 0) {
					pollfd->fd = stream->rfd;
					pollfd->events = POLLIN;
				}
			} else if (revents & POLLHEN) {
disable_stream:
				russ_relay_remove(self, stream->rfd, stream->wfd);
				nactive--;
			}
			nevents--;
		}

		/* special case exitfd */
		if ((pollfds[nstreams].fd == exitfd) && (pollfds[nstreams].revents & POLLHUP)) {
			/* disable exitfd polling */
			pollfds[nstreams].fd = -1;
			nactive--;
			/* remove disable-on-exit streams */
			for (i = 0; i < nstreams; i++) {
				if ((pollfds[i].fd >= 0) && (streams[i]) && (streams[i]->closeonexit)) {
					russ_relay_remove(self, streams[i]->rfd, streams[i]->wfd);
					nactive--;
				}
			}
		}
	}
}
