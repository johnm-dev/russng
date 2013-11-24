/**
* Relay and relaystream objects.
*/

#ifndef RUSS_RELAY2_H
#define RUSS_RELAY2_H

struct russ_relay2stream {
	int		rfd;		/**< read fd */
	int		wfd;		/**< write fd */
	struct russ_buf	*rbuf;		/**< output russ_buf */
	int		auto_close;	/**< close on HEN */
	int		bidir;		/**< flag as bidirectional fds */
};

struct russ_relay2 {
	int				nstreams;
	int				exit_fd;
	struct russ_relay2stream	**streams;
	struct pollfd			*pollfds;
};

/* relay.c */
struct russ_relay2 *russ_relay2_new(int);
struct russ_relay2 *russ_relay2_free(struct russ_relay2 *);
int russ_relay2_add(struct russ_relay2 *, int, int, int, int);
int russ_relay2_add2(struct russ_relay2 *, int, int, int, int);
int russ_relay2_find(struct russ_relay2 *, int, int);
int russ_relay2_remove(struct russ_relay2 *, int, int);
int russ_relay2_poll(struct russ_relay2 *, int);
int russ_relay2_serve(struct russ_relay2 *, int, int);

#endif /* RUSS_RELAY2_H */