/*
* disp.c
*/

#include <errno.h>
#include <memory.h>
#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "disp.h"
#include "russ_priv.h"

#define POLLHEN	(POLLHUP|POLLERR|POLLNVAL)

struct rw *
rw_new(int type, int datafd) {
	struct rw	*self;
	int		p[2];

	if ((self = malloc(sizeof(struct rw))) == NULL) {
		return NULL;
	}
	if (pipe(p) < 0) {
		free(self);
		return NULL;
	}
	self->id = -1;
	self->type = type;
	self->rsigfd = p[0];
	self->wsigfd = p[1];
	self->datafd = datafd;
}

struct rw *
rw_destroy(struct rw *self) {
	if (self->rsigfd >= 0) {
		close(self->rsigfd);
	}
	if (self->wsigfd >= 0) {
		close(self->wsigfd);
	}
	pthread_join(self->th, NULL);
	free(self);
	return NULL;
}

/*
* Read from datafd and send.
*
* Reader alternates between two states:
* 1) waiting to read data
* 2) waiting for ack
*
* HUP on rsigfd is fatal. HUP on datafd sends a HUP.
*/
void *
reader_handler(void *arg) {
	struct rw	*self;
	struct pollfd	pollfds[2];
	char		lbuf[16];

	self = arg;

	pollfds[0].fd = self->rsigfd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = self->datafd;
	pollfds[1].events = POLLIN;

	while (1) {
fprintf(stderr, "reader [%d,%d,%d]: waiting for data\n", self->id, self->datafd, self->rsigfd);
		if (russ_poll(pollfds, 2, -1) < 0) {
fprintf(stderr, "reader [%d,%d,%d]: errno (%d)\n", self->id, self->datafd, self->rsigfd, errno);
			break;
		}
fprintf(stderr, "reader [%d,%d,%d]: revents[0] (%d) revents[1] (%d)\n", self->id, self->datafd, self->rsigfd, pollfds[0].revents, pollfds[1].revents);
		//if ((!(pollfds[0].revents & POLLIN)) && (pollfds[0].revents & POLLHEN)) {
		if (pollfds[0].revents & POLLHEN) {
			break;
		}
fprintf(stderr, "reader [%d,%d,%d]: reading data\n", self->id, self->datafd, self->rsigfd);
		/* read data */
		if ((self->count = russ_read(self->datafd, self->buf, DISPATCHER_BUF_SIZE)) <= 0) {
fprintf(stderr, "reader [%d,%d,%d]: sending HUP\n", self->id, self->datafd, self->rsigfd);
			dispatcher_send(self->disp, self->id, DISPATCHER_HUP, NULL, 0);
			break;
		}
fprintf(stderr, "reader [%d,%d,%d]: sending data ([%d]%20s)\n", self->id, self->datafd, self->rsigfd, self->count, self->buf);
		/* send data */
		if (dispatcher_send(self->disp, self->id, DISPATCHER_DATA, self->buf, self->count) < 0) {
			break;
		}
fprintf(stderr, "reader [%d,%d,%d]: waiting for ack\n", self->id, self->datafd, self->rsigfd);
		/* wait for ack */
		if ((russ_poll(pollfds, 1, -1) < 0)
			|| (pollfds[0].revents & POLLHEN)
			|| (russ_readn(self->rsigfd, lbuf, 1) < 0)) {
			break;
		}
	}
fprintf(stderr, "reader [%d,%d,%d]: shutting down\n", self->id, self->datafd, self->rsigfd);
	close(self->rsigfd);
	close(self->datafd);
	return NULL;
}

/*
* Write data to datafd.
*
* Writer alternates between two states:
* 1) waiting for data
* 2) writing data
*
* HUP on rsigfd is fatal. HUP on datafd sends a HUP.
*/
void *
writer_handler(void *arg) {
	struct rw	*self;
	struct pollfd	pollfds[2];
	char		lbuf[16];
	int		n;

	self = arg;

	pollfds[0].fd = self->rsigfd;
	pollfds[0].events = POLLIN;
	pollfds[1].fd = self->datafd;
	pollfds[1].events = POLLHUP;

	while (1) {
fprintf(stderr, "writer [%d,%d,%d]: waiting for data\n", self->id, self->datafd, self->rsigfd);
		/* wait for data */
		if (russ_poll(pollfds, 2, -1) < 0) {
fprintf(stderr, "writer [%d,%d,%d]: errno (%d)\n", self->id, self->datafd, self->rsigfd, errno);
			break;
		}
fprintf(stderr, "writer [%d,%d,%d]: revents[0] (%d) revents[1] (%d)\n", self->id, self->datafd, self->rsigfd, pollfds[0].revents, pollfds[1].revents);
		//if ((!(pollfds[0].revents & POLLIN)) && (pollfds[0].revents & POLLHEN)) {
		if (pollfds[0].revents & POLLHEN) {
			break;
		}
fprintf(stderr, "writer [%d,%d,%d]: checking rsigfd\n", self->id, self->datafd, self->rsigfd);
		if (pollfds[1].revents & POLLHEN) {
fprintf(stderr, "writer [%d,%d,%d]: sending HUP\n", self->id, self->datafd, self->rsigfd);
			dispatcher_send(self->disp, self->id, DISPATCHER_HUP, NULL, 0);
			break;
		} 
fprintf(stderr, "writer [%d,%d,%d]: reading rsigfd\n", self->id, self->datafd, self->rsigfd);
		if (russ_readn(self->rsigfd, lbuf, 1) < 0) {
			break;
		}
fprintf(stderr, "writer [%d,%d,%d]: writing data ([%d]%20s)\n", self->id, self->datafd, self->rsigfd, self->count, self->buf);
		/* write data */
		if (russ_writen(self->datafd, self->buf, self->count) < 0) {
fprintf(stderr, "writer [%d,%d,%d]: sending HUP\n", self->id, self->datafd, self->rsigfd);
			dispatcher_send(self->disp, self->id, DISPATCHER_HUP, NULL, 0);
			break;
		}
fprintf(stderr, "writer [%d,%d,%d]: sending ack\n", self->id, self->datafd, self->rsigfd);
		/* send ack */
		if (dispatcher_send(self->disp, self->id, DISPATCHER_ACK, NULL, 0) < 0) {
			break;
		}
	}
fprintf(stderr, "writer [%d,%d,%d]: shutting down\n", self->id, self->datafd, self->rsigfd);
	close(self->rsigfd);
	close(self->datafd);
	return NULL;
}

typedef void *(rw_handler)(void *);

int
dispatcher_add_rw(struct dispatcher *self, struct rw *rw) {
	rw_handler	*rwh;

	if ((rw == NULL)
		|| (self->nrws == self->maxrws)) {
		return -1;
	}
	rwh = (rw->type == DISPATCHER_READER) ? reader_handler: writer_handler;
	if (pthread_create(&rw->th, NULL, rwh, (void *)rw) < 0) {
		return -1;
	}
	rw->id = self->nrws;
	rw->disp = self;
	self->rws[self->nrws] = rw;
	self->nrws++;
	self->narws++;

	return 0;
}

struct dispatcher *
dispatcher_new(int maxrws, int fd) {
	struct dispatcher	*self = NULL;

	if (((self = malloc(sizeof(struct dispatcher))) == NULL)
		|| ((self->rws = malloc(sizeof(struct rw *)*maxrws)) == NULL)) {
		free(self);
		return NULL;
	}

	self->msgfd = fd;
	self->nrws = 0;
	self->maxrws = maxrws;
	return self;
}

struct dispatcher *
dispatcher_destroy(struct dispatcher *self) {
	if (self == NULL) {
		return NULL;
	}
	free(self->rws);
	free(self);
	return NULL;
}

int
dispatcher_send(struct dispatcher *self, int id, int mtype, char *buf, int psize) {
	char	hbuf[DISPATCHER_HEADER_BUF_SIZE];
	char	*bp, *bend;
	int	ev;

	ev = 0;
	bp = hbuf;
	bend = hbuf+DISPATCHER_HEADER_BUF_SIZE;
	if ((memset(bp, 0, DISPATCHER_HEADER_BUF_SIZE) == NULL)
		|| ((bp = russ_enc_i(bp, bend, id)) == NULL)
		|| ((bp = russ_enc_i(bp, bend, mtype)) == NULL)
		|| ((bp = russ_enc_i(bp, bend, psize)) == NULL)) {
		return -1;
	}
	
	if (pthread_mutex_lock(&self->wlock) < 0) {
		return -1;
	}
	if ((russ_writen(self->msgfd, hbuf, DISPATCHER_HEADER_BUF_SIZE) < 0)
		|| (russ_writen(self->msgfd, buf, psize) < 0)) {
		ev = -1;
	}
	if (pthread_mutex_unlock(&self->wlock) < 0) {
		ev = -1;
	}
	return ev;
}

void
dispatcher_loop(struct dispatcher *self) {
	struct rw	*rw;
	char		hbuf[DISPATCHER_HEADER_BUF_SIZE];
	int		id, mtype, psize;
	char		*bp, *bend;
	int		i;

	while (self->narws > 0) {
		/* get message header */
		bp = hbuf;
		if ((russ_readn(self->msgfd, bp, DISPATCHER_HEADER_BUF_SIZE) <= 0)
			|| ((bp = russ_dec_i(bp, &id)) == NULL)
			|| ((bp = russ_dec_i(bp, &mtype)) == NULL)
			|| ((bp = russ_dec_i(bp, &psize)) == NULL)) {
			break;
		}

		/* get message data and signal rw */
		rw = self->rws[id];
		rw->count = psize;
		if (mtype == DISPATCHER_HUP) {
fprintf(stderr, "loop [%d,%d,%d]: HUP\n", id, rw->datafd, rw->rsigfd);
			close(rw->wsigfd);
			rw->wsigfd = -1;
			self->narws--;
		} else if (((psize > 0) & (russ_readn(self->msgfd, rw->buf, psize) <= 0))
			|| (fprintf(stderr, "loop [%d,%d,%d]: rw->type (%d) mtype (%d) psize (%d) buf (%20s)\n", id, rw->datafd, rw->rsigfd, rw->type, mtype, psize, rw->buf) < 0)
			|| (russ_writen(rw->wsigfd, "x", 1) < 0)) {
			break;
		}
	}
fprintf(stderr, "loop: shutdown\n");

	/* close wsigfds */
	for (i = 0; i < self->nrws; i++) {
		rw = self->rws[i];
		if (rw->wsigfd >= 0) {
			close(rw->wsigfd);
			rw->wsigfd = -1;
		}
	}
	close(self->msgfd);
	self->msgfd = -1;
}
