/*
* include/russ_relay.h
*/

#ifndef RUSS_RELAY_H
#define RUSS_RELAY_H

#include <russ.h>

#define RUSS_RELAY_BUFSIZE	(2<<15)

#define RUSS_RELAYDIR_WE	0x1
#define RUSS_RELAYDIR_EW	0x2
#define RUSS_RELAYDIR_BI	RUSS_RELAYDIR_WE|RUSS_RELAYDIR_EW

struct russ_relaydata {
	int		fd;		/**< associated fd */
	struct russ_buf	*rbuf;		/**< output russ_buf */
	int		auto_close;	/**< close on HEN */
};

struct russ_relay {
	int			nfds;
	struct pollfd		*pollfds;
	struct russ_relaydata	**rdatas;
};

struct russ_relay *russ_relay_new(int);
struct russ_relay *russ_relay_free(struct russ_relay *);
int russ_relay_add(struct russ_relay *, int, int, int, int, int, int, int);
int russ_relay_remove(struct russ_relay *, int);
int russ_relay_poll(struct russ_relay *, int);
int russ_relay_serve(struct russ_relay *, int);

#endif /* RUSS_RELAY_H */