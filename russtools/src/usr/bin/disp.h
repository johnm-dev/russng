/*
* disp.h
*/

#ifndef _RW_HEADER
#define _RW_HEADER

#include <pthread.h>

#define DISPATCHER_HEADER_BUF_SIZE	16
#define DISPATCHER_BUF_SIZE		32768
#define DISPATCHER_MAX_RWS		16

#define DISPATCHER_DATA		1
#define DISPATCHER_ACK		2
#define DISPATCHER_HUP		3

#define DISPATCHER_READER	1
#define DISPATCHER_WRITER	2

struct rw {
	pthread_t		th;		/**< thread */
	struct dispatcher	*disp;		/**< dispatcher object */
	int			type;		/**< reader/writer */
	int			id;		/**< stream id; corresponds to writer */
	int			rsigfd, wsigfd;	/**< read and write ends of a signal pipe */
	int			datafd;		/**< input/ouput data fd; -1 if closed */
	char			buf[DISPATCHER_BUF_SIZE];	/**< data buffer; used for input and output */
	int			count;		/**< # of bytes in buffer */
};

struct dispatcher {
	pthread_mutex_t		rlock, wlock;
	int			msgfd;
	struct rw		**rws;
	int			nrws;		/**< number of rws */
	int			narws;		/**< number of active rws */
	int			maxrws;
};

struct rw *rw_new(int type, int datafd);
struct rw *rw_destroy(struct rw *self);
int dispatcher_add_rw(struct dispatcher *self, struct rw *rw);
struct dispatcher *dispatcher_new(int maxrws, int mfd);
struct dispatcher *dispatcher_destroy(struct dispatcher *self);
void *reader_handler(void *arg);
void *writer_handler(void *arg);
int dispatcher_send(struct dispatcher *self, int id, int mtype, char *buf, int psize);
void dispatcher_loop(struct dispatcher *self);

#endif /* _RW_HEADER */
