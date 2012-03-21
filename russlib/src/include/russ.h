/*
** include/russ.h
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

#ifndef RUSS_H
#define RUSS_H

#include <sys/types.h>
#include <unistd.h>

/*
#define DEBUG
*/
#ifdef DEBUG
#define dprintf(...)	fprintf(stderr, __VA_ARGS__)
#else
#define dprintf(...)
#endif

#define RUSS_MAX_ATTRC		1024
#define RUSS_MAX_ARGC		1024
#define RUSS_PROTOCOL_STRING	"0004"
#define RUSS_SERVICE_DIR	"/srv/russ"

/**
* Client credentials object. Obtained from connection.
*/
struct russ_credentials {
	long	pid;
	long	uid;
	long	gid;
};

/**
* Listener object.
*/
struct russ_listener {
	int	sd;	/**< socket descriptor */
};

/**
* Request object.
*/
struct russ_request {
	char	*protocol_string;	/**< identifies russ protocol */
	char	*spath;		/**< service path */
	char	*op;		/**< operation string */
	int	attrc;		/**< # of attrs */
	char	**attrv;	/**< NULL terminated array of attributes (as name=value strings) */
	int	argc;		/**< # of args */
	char	**argv;		/**< array of args */
};

/**
* Connection object. Shared by client and server.
*/
struct russ_conn {
	int			conn_type;	/**< client or server */
	struct russ_credentials	cred;		/**< credentials */
	struct russ_request	req;		/**< request */
	int			sd;		/**< socket descriptor */
	int			fds[3];		/**< std{in,out,err} */
};

/**
* Container for byte forwarding information.
*/
struct russ_forwarding {
	pthread_t	th;		/**< thread doing forwarding */
	int		to_join;	/**< 1 to join thread */
	int		in_fd;		/**< input fd */
	int		out_fd;		/**< output fd */
	int		count;		/**< # of bytes to forward */
	int		blocksize;	/**< max size of blocks at once */
	int		how;		/**< 0 for normal read, 1 for readline */
};

/* TODO: is this necessary */
struct pipe_fds {
	int in_fd, out_fd;
};

typedef int (*russ_req_handler)(struct russ_conn *);

char *russ_find_service_addr(char *);

/* conn.c */
void russ_close_fd(struct russ_conn *, int);

struct russ_conn *russ_dialv(char *, char *, int, char **, int, char **);
struct russ_conn *russ_diall(char *, char *, int, char **, ...);
void russ_close_conn(struct russ_conn *);
struct russ_conn *russ_free_conn(struct russ_conn *);

struct russ_listener *russ_announce(char *, mode_t, uid_t, gid_t);
struct russ_conn *russ_answer(struct russ_listener *, int);
int russ_accept(struct russ_conn *, int *, int *);
void russ_close_listener(struct russ_listener *);
struct russ_listener *russ_free_listener(struct russ_listener *);

/* io.c */
ssize_t russ_read(int, char *, size_t);
ssize_t russ_readline(int, char *, size_t);
ssize_t russ_readn(int, char *, size_t);
ssize_t russ_writen(int, char *, size_t);
ssize_t russ_writen_timeout(int, char *, size_t, int);

int russ_stream_fd(int, int, long, long);
void russ_forwarding_init(struct russ_forwarding *, int, int, int, int, int, int);
int russ_forward_bytes(int, struct russ_forwarding *);

/* misc.c */
int russ_dprintf(int, char *, ...);

/* request.c */
int russ_await_request(struct russ_conn *);

/* server.c */
void russ_loop(struct russ_listener *, russ_req_handler);

#endif /* RUSS_H */
