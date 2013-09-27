/*
** forwarder.c
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
#include <time.h>

#include "russ_priv.h"

struct pipe_fds {
	int in_fd, out_fd;
};

/*
* Forward a block by bunch of bytes or line.
*
* Reading is of bytes available (thus russ_read()). However, writing
* must be of bytes read (thus russ_writen()).
*
* @param in_fd		input fd
* @param out_fd		output fd
* @param buf		buffer
* @param bsize		buffer size
* @param how		0 for bunch, 1 for line
* @return		# of bytes forwarded; -1 on failure
*/
static int
_forward_block(int in_fd, int out_fd, char *buf, int bsize, int how) {
	long	nread;

	if (how == 0) {
		nread = russ_read(in_fd, buf, bsize);
	} else {
		nread = russ_readline(in_fd, buf, bsize);
	}
	if (nread < 0) {
		return -1;
	} else if (nread == 0) {
		/* EOF */
		return 0;
	} else {
		if (russ_writen(out_fd, buf, nread) != nread) {
			/* short write */
			return -1;
		}
	}
	return nread;
}

/*
* The actual byte forwarder code.
*
* @param fwd		forwarder object
* @return		NULL on success; !NULL on failure
*/
static void *
_forward_bytes(void *_fwd) {
	struct russ_fwd	*fwd;
	char			buf[RUSS_FWD_BUF_MAX], *bp;
	long			nread, nwrite, count;
	int			rv = 0;

	/* setup */
	fwd = (struct russ_fwd *)_fwd;
	if (fwd->blocksize <= RUSS_FWD_BUF_MAX) {
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
	struct russ_fwd	*fwd;
	struct pollfd		pollfds[2];
	char			buf[RUSS_FWD_BUF_MAX], *bp;
	long			nread, nwrite, count;
	int			rv;

	fwd = (struct russ_fwd *)_fwd;
	fwd->reason = 0;

	if (fwd->blocksize <= RUSS_FWD_BUF_MAX) {
		bp = buf;
	} else {
		if ((bp = malloc(fwd->blocksize)) == NULL) {
			return NULL;
		}
	}

	pollfds[0].fd = fwd->in_fd;
	pollfds[0].events = POLLIN|POLLERR;
	pollfds[1].fd = fwd->out_fd;
	pollfds[1].events = POLLHUP|POLLERR;

	while (1) {
		if ((rv = poll(pollfds, 2, -1)) <= 0) {
			if (rv == 0) {
				fwd->reason = RUSS_FWD_REASON_TIMEOUT;
			} else {
				fwd->reason = RUSS_FWD_REASON_ERROR;
			}
			break;
		}
		if ((pollfds[0].revents & POLLERR) || (pollfds[1].revents & POLLERR)) {
			fwd->reason = RUSS_FWD_REASON_ERROR;
			break;
		}
		if (pollfds[1].revents & POLLHUP) {
			fwd->reason = RUSS_FWD_REASON_OUT_HUP;
			break;
		}
		if (pollfds[0].revents & POLLIN) {
			rv = _forward_block(fwd->in_fd, fwd->out_fd, bp, fwd->blocksize, fwd->how);
			if (rv < 0) {
				fwd->reason = RUSS_FWD_REASON_ERROR;
				break;
			} else if (rv == 0) {
				fwd->reason = RUSS_FWD_REASON_EOF;
				break;
			}
			continue;
		} else if (pollfds[0].revents & POLLHUP) {
			fwd->reason = RUSS_FWD_REASON_IN_HUP;
			break;
		}
	}
	if ((fwd->close_fds) && (fwd->reason < 0)) {
		if (fwd->close_fds & RUSS_FWD_CLOSE_IN) {
			russ_close(fwd->in_fd);
		}
		if (fwd->close_fds & RUSS_FWD_CLOSE_OUT) {
			russ_close(fwd->out_fd);
		}
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
russ_fwd_init(struct russ_fwd *self, int id, int in_fd, int out_fd, int count, int blocksize, int how, int close_fds) {
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
* @param fwds		array of (initialized) forwarders
* @param nfwds		# of forwarders
* @return		0 on success; -1 on failure
*/
int
russ_fwds_run(struct russ_fwd *fwds, int nfwds) {
	pthread_attr_t	attr;
	int		i;

	/* set up/start threads */
	for (i = 0; i < nfwds; i++) {
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 2*RUSS_FWD_BUF_MAX+(1<<20));
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
russ_fwd_start(struct russ_fwd *self) {
	pthread_attr_t	attr;

	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 2*RUSS_FWD_BUF_MAX+(1<<20));
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
russ_fwd_join(struct russ_fwd *self) {
	pthread_join(self->th, NULL);
}

#if 0
/*
** forward bytes between in fd and out fd
**
** Linux: optimized using zero-copy splice()
*/
void
russ_forward_bytes(int fd_in, int fd_out) {
#ifdef x_GNU_SOURCE
	int	cnt;

	while (1) {
/*fprintf(stderr, "calling splice\n");*/
		cnt = splice(fd_in, NULL, fd_out, NULL, RUSS_FWD_BUF_MAX, 0);
	}
#else /* _GNU_SOURCE */
	char	buf[RUSS_FWD_BUF_MAX];
	int	cnt, rn, wn;

	while (1) {
		rn = read(fd_in, buf, RUSS_FWD_BUF_MAX);
/*fprintf(stderr, "rn (%d) buf (%20s)\n", rn, buf);*/
		if (rn > 0) {
			for (wn = 0; wn < rn; ) {
				cnt = write(fd_out, &buf[wn], rn-wn);
				wn += cnt;
			}
		} else if (rn < 0) {
			/* error */
			if (errno != EINTR) {
				break;
			}
		} else {
			/* EOF */
			break;
		}
	}
#endif /* _GNU_SOURCE */

/*fprintf(stderr, "exiting russ_forward_bytes\n");*/
	close(fd_in);
	close(fd_out);
}

void *
russ_forward_bytes_vp(void *vp) {
	struct pipe_fds	*pfds;

	pfds = (struct pipe_fds *)vp;
	russ_forward_bytes(pfds->in_fd, pfds->out_fd);
}
#endif