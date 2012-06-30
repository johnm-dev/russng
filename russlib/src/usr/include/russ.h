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

#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>
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

#define RUSS_CONN_NFDS		3
#define RUSS_MAX_SPATH_LEN	8192
#define RUSS_MAX_ATTRC		1024
#define RUSS_MAX_ARGC		1024
#define RUSS_PROTOCOL_STRING	"0006"
#define RUSS_SERVICES_DIR	"/var/run/russ/services"

#define RUSS_TIMEOUT_NEVER	-1
#define RUSS_TIMEOUT_NOW	0

/* common messages */
#define RUSS_MSG_BAD_OP		"error: unsupported operation"
#define RUSS_MSG_NO_EXIT	"error: no exit status"
#define RUSS_MSG_NO_SERVICE	"error: no service"

/* common exit_status values */
#define RUSS_EXIT_SUCCESS	0
#define RUSS_EXIT_FAILURE	-1
#define RUSS_EXIT_CALL_FAILURE	-126
#define RUSS_EXIT_SYS_FAILURE	-127

/**
* Target of service.
*/
struct russ_target {
	char	saddr[RUSS_MAX_SPATH_LEN];
	char	spath[RUSS_MAX_SPATH_LEN];
};

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
	char	*op;		/**< operation string */
	char	*spath;		/**< service path */
	char	**attrv;	/**< NULL-terminated array of attributes (as name=value strings) */
	char	**argv;		/**< NULL-terminated array of args */
};

/**
* Connection object. Shared by client and server.
*/
struct russ_conn {
	int			conn_type;	/**< client or server */
	struct russ_credentials	cred;		/**< credentials */
	struct russ_request	req;		/**< request */
	int			sd;		/**< socket descriptor */
	int			exit_fd;	/**< for exit status */
	int			nfds;		/**< # of helper fds */
	int			fds[RUSS_CONN_NFDS];		/**< array of fds */
};

/**
* Forwarder information.
*/
struct russ_forwarder {
	pthread_t	th;		/**< thread doing forwarding */
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

typedef int64_t	russ_timeout;

typedef void (*russ_req_handler)(struct russ_conn *);

/* addr.c */
struct russ_target *russ_find_service_target(char *);
char *russ_resolve_addr(char *);

/* conn.c */
int russ_conn_accept(struct russ_conn *, int *, int *);
int russ_conn_await_request(struct russ_conn *);
void russ_conn_close(struct russ_conn *);
void russ_conn_close_fd(struct russ_conn *, int);
int russ_conn_exit(struct russ_conn *, int);
int russ_conn_fatal(struct russ_conn *, char *, int);
struct russ_conn *russ_conn_free(struct russ_conn *);
int russ_conn_wait(struct russ_conn *, int *, russ_timeout);

struct russ_conn *russ_dialv(russ_timeout, char *, char *, char **, char **);
struct russ_conn *russ_diall(russ_timeout, char *, char *, char **, ...);

/* forwarder */
void russ_forwarder_init(struct russ_forwarder *, int, int, int, int, int);
int russ_forwarder_start(struct russ_forwarder *);
int russ_forwarder_join(struct russ_forwarder *);
int russ_run_forwarders(int, struct russ_forwarder *);

/* helpers.c */
struct russ_conn *russ_execv(russ_timeout, char *, char **, char **);
struct russ_conn *russ_execl(russ_timeout, char *, char **, ...);
struct russ_conn *russ_help(russ_timeout, char *);
struct russ_conn *russ_info(russ_timeout, char *);
struct russ_conn *russ_list(russ_timeout, char *);

/* io.c */
ssize_t russ_read(int, char *, size_t);
ssize_t russ_readline(int, char *, size_t);
ssize_t russ_readn(int, char *, size_t);
ssize_t russ_writen(int, char *, size_t);
ssize_t russ_writen_timeout(int, char *, size_t, russ_timeout);

/* listener.c */
struct russ_listener *russ_announce(char *, mode_t, uid_t, gid_t);
struct russ_conn *russ_listener_answer(struct russ_listener *, russ_timeout);
void russ_listener_close(struct russ_listener *);
struct russ_listener *russ_listener_free(struct russ_listener *);

/* misc.c */
int russ_dprintf(int, char *, ...);
int russ_sarray0_count(char **, int);
char **russ_sarray0_dup(char **, int);

/* request.c */

/* server.c */
void russ_loop(struct russ_listener *, russ_req_handler);
void russ_loop_thread(struct russ_listener *, russ_req_handler);

#endif /* RUSS_H */
