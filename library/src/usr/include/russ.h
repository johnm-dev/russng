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

#include <limits.h>
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

#define RUSS_CONN_NFDS		32
#define RUSS_CONN_MIN_NFDS	4
#define RUSS_MAX_PATH_LEN	8192
#define RUSS_MAX_ATTRC		1024
#define RUSS_MAX_ARGC		1024
#define RUSS_PROTOCOL_STRING	"0008"
#define RUSS_SERVICES_DIR	"/var/run/russ/services"

#define RUSS_DEADLINE_NEVER	INT64_MAX

/* common messages */
#define RUSS_MSG_BAD_ARGS	"error: bad/missing arguments"
#define RUSS_MSG_BAD_OP		"error: unsupported operation"
#define RUSS_MSG_NO_DIAL	"error: cannot dial service"
#define RUSS_MSG_NO_EXIT	"error: no exit status"
#define RUSS_MSG_NO_SERVICE	"error: no service"
#define RUSS_MSG_UNDEF_SERVICE	"warning: undefined service"

/* common exit_status values */
#define RUSS_EXIT_SUCCESS	0
#define RUSS_EXIT_FAILURE	-1
#define RUSS_EXIT_CALL_FAILURE	-126
#define RUSS_EXIT_SYS_FAILURE	-127

/* forwarder reasons */
#define RUSS_FWD_REASON_EOF	0
#define RUSS_FWD_REASON_ERROR	-1
#define RUSS_FWD_REASON_TIMEOUT	-2
#define RUSS_FWD_REASON_COUNT	-3
#define RUSS_FWD_REASON_IN_HUP	-4
#define RUSS_FWD_REASON_OUT_HUP	-5

/**
* Target of service.
*/
struct russ_target {
	char	saddr[RUSS_MAX_PATH_LEN];
	char	spath[RUSS_MAX_PATH_LEN];
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
	int			fds[RUSS_CONN_NFDS];		/**< array of fds */
};

/**
* Forwarder information.
*/
struct russ_forwarder {
	pthread_t	th;		/**< thread doing forwarding */
	int		id;		/**< configurable id */
	int		in_fd;		/**< input fd */
	int		out_fd;		/**< output fd */
	int		count;		/**< # of bytes to forward */
	int		blocksize;	/**< max size of blocks at once */
	int		how;		/**< 0 for normal read, 1 for readline */
	int		close_fds;	/**< 1 to close fds before returning */
	int		reason;		/**< reason forwarder returned */
};

/* TODO: is this necessary */
struct pipe_fds {
	int in_fd, out_fd;
};

typedef int64_t russ_deadline;

typedef void (*russ_req_handler)(struct russ_conn *);
typedef void (*russ_accept_handler)(struct russ_conn *, int, int *, int *);

/* addr.c */
struct russ_target *russ_find_service_target(char *);
char *russ_resolve_addr(char *);

/* conn.c */
int russ_conn_accept(struct russ_conn *, int, int *, int *);
int russ_conn_await_request(struct russ_conn *, russ_deadline);
void russ_conn_close(struct russ_conn *);
void russ_conn_close_fd(struct russ_conn *, int);
int russ_conn_exit(struct russ_conn *, int);
int russ_conn_fatal(struct russ_conn *, char *, int);
struct russ_conn *russ_conn_free(struct russ_conn *);
int russ_conn_sendfds(struct russ_conn *, int, int *, int *);
int russ_conn_splice(struct russ_conn *, struct russ_conn *);
int russ_conn_wait(struct russ_conn *, int *, russ_deadline);

struct russ_conn *russ_dialv(russ_deadline, char *, char *, char **, char **);
struct russ_conn *russ_diall(russ_deadline, char *, char *, char **, ...);

/* forwarder */
void russ_forwarder_init(struct russ_forwarder *, int, int, int, int, int, int, int);
int russ_forwarder_start(struct russ_forwarder *);
int russ_forwarder_join(struct russ_forwarder *);
int russ_run_forwarders(int, struct russ_forwarder *);

/* helpers.c */
struct russ_conn *russ_execv(russ_deadline, char *, char **, char **);
struct russ_conn *russ_execl(russ_deadline, char *, char **, ...);
struct russ_conn *russ_help(russ_deadline, char *);
struct russ_conn *russ_info(russ_deadline, char *);
struct russ_conn *russ_list(russ_deadline, char *);

/* io.c */
ssize_t russ_read(int, char *, size_t);
ssize_t russ_readline(int, char *, size_t);
ssize_t russ_readn(int, char *, size_t);
ssize_t russ_readn_deadline(int, char *, size_t, russ_deadline);
ssize_t russ_writen(int, char *, size_t);
ssize_t russ_writen_deadline(int, char *, size_t, russ_deadline);

/* listener.c */
struct russ_listener *russ_announce(char *, mode_t, uid_t, gid_t);
struct russ_conn *russ_listener_answer(struct russ_listener *, russ_deadline);
void russ_listener_close(struct russ_listener *);
struct russ_listener *russ_listener_free(struct russ_listener *);
void russ_listener_loop(struct russ_listener *, russ_accept_handler, russ_req_handler);

/* misc.c */
int russ_dprintf(int, char *, ...);
int russ_sarray0_count(char **, int);
char **russ_sarray0_dup(char **, int);
int russ_switch_user(uid_t, gid_t, int, gid_t *);
int russ_unlink(char *);

/* request.c */

/* time.c */
inline russ_deadline russ_gettime(void); /* internal */
inline russ_deadline russ_to_deadline(int);
inline russ_deadline russ_to_deadline_diff(russ_deadline);
inline int russ_to_timeout(russ_deadline deadline);

#endif /* RUSS_H */
