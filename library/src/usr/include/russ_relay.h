/*
* include/russ_relay.h
*/

#ifndef RUSS_RELAY_H
#define RUSS_RELAY_H

#define RUSS_RELAY_BUFSIZE	(2<<15)

#define RUSS_RELAYDIR_WE	0x1
#define RUSS_RELAYDIR_EW	0x2
#define RUSS_RELAYDIR_BI	RUSS_RELAYDIR_WE|RUSS_RELAYDIR_EW

struct russ_relaydata {
	int		fd;		/**< associated fd */
	char		*buf;		/**< output buffer */
	int		bufcnt;		/**< # of bytes in buffer */
	int		bufoff;		/**< buffer offset */
	int		bufsize;	/**< buffer size */
	int		auto_close;	/**< close on HEN */
};

struct russ_relay {
	int			nfds;
	struct pollfd		*pollfds;
	struct russ_relaydata	*datas;
};

struct russ_relay *russ_relay_new(int);
struct russ_relay *russ_relay_destroy(struct russ_relay *);
int russ_relay_add(struct russ_relay *, int, int, int, int, int, int, int);
int russ_relay_remove(struct russ_relay *, int);
int russ_relay_poll(struct russ_relay *, int);
int russ_relay_serve(struct russ_relay *, int);

#endif /* RUSS_RELAY_H */