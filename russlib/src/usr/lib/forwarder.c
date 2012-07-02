/*
** forwarder.c
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

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "russ_priv.h"

/*
* Forward a block by bunch of bytes or line.
*
* @param in_fd		input fd
* @param out_fd		output fd
* @param buf		buffer
* @param bsize		buffer size
* @param how		0 for bunch, 1 for line
* @return		# of bytes forwarded, -1 on failure
*/
static int
_forward_block(int in_fd, int out_fd, char *buf, int bsize, int how) {
	long	nread, nwrite;

	if (how == 0) {
		if ((nread = russ_read(in_fd, buf, bsize)) < 0) {
			return nread;
		}
	} else {
		if ((nread = russ_readline(in_fd, buf, bsize)) < 0) {
			return nread;
		}
	}
	if (nread == 0) {
		return 0;
	}
	if ((nwrite = russ_writen(out_fd, buf, nread)) != nread) {
		return nwrite;
	}
	return nwrite;
}

/*
* The actual byte forwarder code.
*
* @param fwd		forwarder object
* @return		NULL on success; !NULL on failure
*/
static void *
_forward_bytes(void *_fwd) {
	struct russ_forwarder	*fwd;
	char			buf[1<<20], *bp;
	long			nread, nwrite, count;
	int			rv = 0;

	/* setup */
	fwd = (struct russ_forwarder *)_fwd;
	if (fwd->blocksize <= 1<<20) {
		bp = buf;
	} else {
		if ((bp = malloc(fwd->blocksize)) == NULL) {
			return NULL;
		}
	}

	/* transfer */
	if (fwd->count == -1) {
		while (1) {
			if ((rv = _forward_block(fwd->in_fd, fwd->out_fd, bp, fwd->blocksize, fwd->how)) <= 0) {
				break;
			}
		}
	} else {
		count = fwd->count;
		while (count > 0) {
			nread = (fwd->blocksize < count) ? fwd->blocksize : count;
			if ((rv = _forward_block(fwd->in_fd, fwd->out_fd, bp, nread, fwd->how)) <= 0) {
				break;
			}
			count -= nread;
		}
	}

	/* release */
	if (bp != buf) {
		free(bp);
	}
	if (rv) {
		return (void *)!NULL;
	}
	return NULL;
}

/**
* Run actual byte forwarder (poll-based).
*
* Forward bytes from in_fd to out_fd. poll() is used to sense state
* of in_fd and out_fd allowing for return on timeout, error, hup on
* in_fd, hup on out_fd. fwd.reason is set to allow caller to deal
* with fds appropriately.
*
* @param fwd		forwarder object
* @return		forwarder exit reason
*/
static void *
_forward_bytes2(void *_fwd) {
	struct russ_forwarder	*fwd;
	struct pollfd		pollfds[2];
	char			buf[1<<20], *bp;
	long			nread, nwrite, count;

	fwd = (struct russ_forwarder *)_fwd;
	fwd->reason = 0;

	if (fwd->blocksize <= 1<<20) {
		bp = buf;
	} else {
		if ((bp = malloc(fwd->blocksize)) == NULL) {
			return NULL;
		}
	}

	pollfds[0].fd = fwd->in_fd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = fwd->out_fd;
	pollfds[1].events = POLLHUP;

	while (1) {
		if ((rv = poll(pollfds, 2, -1)) <= 0) {
			if (rv == 0) {
				fwd->reason = RUSS_FWD_REASON_TIMEOUT;
			} else {
				fwd->reason = RUSS_FWD_REASON_ERROR;
			}
			break;
		}
		if (pollfds[0].revents & POLLIN) {
			if (_forward_block(fwd->in_fd, fwd->out_fd, bp, fwd->blocksize, fwd->how) < 0) {
				fwd->reason = RUSS_FWD_REASON_ERROR;
				break;
			}
			continue;
		} else if (pollfds[0].revents & POLLHUP) {
			fwd->reason = RUSS_FWD_REASON_IN_HUP;
			break;
		}
		if (pollfds[1].revents & POLLHUP) {
			fwd->reason = RUSS_FWD_REASON_OUT_HUP;
			break;
		}
	}
	if ((fwd->close_fds) && (fwd->reason < 0)) {
		close(fwd->in_fd);
		close(fwd->out_fd);
	}
}

/**
* Initializes forwarder struct with values.
*
* The forwarder struct holds settings used to carry out the
* forwarding operation. The when started, the forwarder forwards
* bytes between in_fd and out_fd with a blocksize up to a total of
* count bytes (-1 is infinite). how determines if the reading of
* in_fd is by block or line. If close_fds is 1, the in_fd and out_fd
* will be closed before returning. This means that no further
* operations should be done on them.
*
* @param self		forwarder object
* @param id		for id member
* @param in_fd		for in_fd member
* @param out_fd		for out_fd member
* @param count		for count member
* @param blocksize	for blocksize member
* @param how		for the how member
* @param close_fds	for close_fds member
*/
void
russ_forwarder_init(struct russ_forwarder *self, int id, int in_fd, int out_fd, int count, int blocksize, int how, int close_fds) {
	self->id = id;
	self->in_fd = in_fd;
	self->out_fd = out_fd;
	self->count = count;
	self->blocksize = blocksize;
	self->how = how;
	self->close_fds = close_fds;
	self->reason = 0;
}

/**
* Run forwarders.
*
* One or more forwarders are started, each in its own thread, to
* forward data between fds.
*
* @param nfwds		# of forwarders
* @param fwds		array of (initialized) forwarders
* @return		0 on success; -1 on failure
*/
int
russ_run_forwarders(int nfwds, struct russ_forwarder *fwds) {
	pthread_attr_t	attr;
	int		i;

	/* set up/start threads */
	for (i = 0; i < nfwds; i++) {
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, (1<<20)+(1<<20));
		if (pthread_create(&(fwds[i].th), &attr, _forward_bytes2, (void *)&(fwds[i])) < 0) {
			goto kill_threads;
		}
		pthread_attr_destroy(&attr);
	}
	return 0;
kill_threads:
	for (i--; i >= 0; i++) {
		pthread_cancel(fwds[i].th);
	}
	return -1;
}

/**
* Run a forwarder.
*
* The forwarder is run in its own thread.
*
* @param self		forwarder object
* @return		0 on success; -1 on failure
*/
int
russ_forwarder_start(struct russ_forwarder *self) {
	pthread_attr_t	attr;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, (1<<20)*2);
	if (pthread_create(&(self->th), &attr, _forward_bytes, (void *)self) < 0) {
		return -1;
	}
	pthread_attr_destroy(&attr);
	return 0;
}

/**
* Join forwarder.
*
* Waits on the thread running the forwarder.
*
* @param self		forwarder object
*/
int
russ_forwarder_join(struct russ_forwarder *self) {
	pthread_join(self->th, NULL);
}
