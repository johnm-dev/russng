/*
** include/russ.h
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
#define RUSS_CONN_STD_NFDS	4
#define RUSS_CONN_FD_STDIN	0
#define RUSS_CONN_FD_STDOUT	1
#define RUSS_CONN_FD_STDERR	2
#define RUSS_CONN_FD_EXIT	3

#define RUSS_DEADLINE_NEVER	INT64_MAX

/* common exit_status values */
#define RUSS_EXIT_SUCCESS	0
#define RUSS_EXIT_FAILURE	1
#define RUSS_EXIT_CALL_FAILURE	126
#define RUSS_EXIT_SYS_FAILURE	127

/* common messages */
#define RUSS_MSG_BAD_ARGS	"error: bad/missing arguments"
#define RUSS_MSG_BAD_CONN_EVENT	"error: unexpected connection event"
#define RUSS_MSG_BAD_OP		"error: unsupported operation"
#define RUSS_MSG_NO_DIAL	"error: cannot dial service"
#define RUSS_MSG_NO_EXIT	"error: no exit status"
#define RUSS_MSG_NO_SERVICE	"error: no service"
#define RUSS_MSG_NO_SWITCH_USER	"error: cannot switch user"
#define RUSS_MSG_UNDEF_SERVICE	"warning: undefined service"

#define RUSS_REQ_ARGS_MAX	1024
#define RUSS_REQ_ATTRS_MAX	1024
#define RUSS_REQ_SPATH_MAX	8192
#define RUSS_REQ_PROTOCOL_STRING	"0009"

#define RUSS_OPNUM_NOT_SET	0
#define RUSS_OPNUM_EXTENSION	1
#define RUSS_OPNUM_EXECUTE	2
#define RUSS_OPNUM_HELP		3
#define RUSS_OPNUM_ID		4
#define RUSS_OPNUM_INFO		5
#define RUSS_OPNUM_LIST		6

#define RUSS_SVR_TIMEOUT_ACCEPT	INT_MAX
#define RUSS_SVR_TIMEOUT_AWAIT	15000

#define RUSS_SVR_TYPE_FORK	1
#define RUSS_SVR_TYPE_THREAD	2

#define RUSS_SERVICES_DIR	"/var/run/russ/services"

typedef uint32_t	russ_opnum;

/**
* Client credentials object. Obtained from connection.
*/
struct russ_creds {
	long	pid;
	long	uid;
	long	gid;
};

/**
* Listener object.
*/
struct russ_lis {
	int	sd;	/**< socket descriptor */
};

/**
* Request object.
*/
struct russ_req {
	char		*protocol_string;	/**< identifies russ protocol */
	char		*op;		/**< operation string */
	russ_opnum	opnum;		/**< operation number*/
	char		*spath;		/**< service path */
	char		**attrv;	/**< NULL-terminated array of attributes (as name=value strings) */
	char		**argv;		/**< NULL-terminated array of args */
};

/**
* Connection object. Shared by client and server.
*/
struct russ_conn {
	int			conn_type;	/**< client or server */
	struct russ_creds	creds;		/**< credentials */
	int			sd;		/**< socket descriptor */
	int			fds[RUSS_CONN_NFDS];		/**< array of fds */
};

/* declare here, defined below */
struct russ_sess;

typedef void (*russ_svchandler)(struct russ_sess *);
typedef int64_t russ_deadline;

typedef struct russ_conn *(*russ_accepthandler)(struct russ_lis *, russ_deadline);
typedef int (*russ_answerhandler)(struct russ_conn *);
typedef void (*russ_reqhandler)(struct russ_conn *);

/**
* Service node object.
*/
struct russ_svcnode {
	russ_svchandler		handler;
	char			*name;
	struct russ_svcnode	*next;
	struct russ_svcnode	*children;
	int			auto_answer;
	int			virtual;
};

/**
* Server object.
*/
struct russ_svr {
	struct russ_svcnode	*root;
	int			type;
	char			*saddr;
	mode_t			mode;
	uid_t			uid;
	gid_t			gid;
	struct russ_lis		*lis;
	russ_accepthandler	accept_handler;
	int			accept_timeout;
	russ_answerhandler	answer_handler;
	int			await_timeout;
	int			auto_switch_user;
};

/**
* Session object
*/
struct russ_sess {
	struct russ_svr		*svr;
	struct russ_conn	*conn;
	struct russ_req		*req;
	char			spath[RUSS_REQ_SPATH_MAX];
};

/* conn.c */
int russ_conn_answer(struct russ_conn *, int, int *, int *);
struct russ_req *russ_conn_await_request(struct russ_conn *, russ_deadline);
void russ_conn_close(struct russ_conn *);
void russ_conn_close_fd(struct russ_conn *, int);
int russ_conn_exit(struct russ_conn *, int);
int russ_conn_exits(struct russ_conn *, char *, int);
int russ_conn_fatal(struct russ_conn *, char *, int);
struct russ_conn *russ_conn_free(struct russ_conn *);
int russ_conn_sendfds(struct russ_conn *, int, int *, int *);
int russ_conn_splice(struct russ_conn *, struct russ_conn *);
int russ_conn_wait(struct russ_conn *, int *, russ_deadline);

struct russ_conn *russ_dialv(russ_deadline, char *, char *, char **, char **);
struct russ_conn *russ_diall(russ_deadline, char *, char *, char **, ...);

/* handlers.c */
struct russ_conn *russ_standard_accept_handler(struct russ_lis *, russ_deadline);
int russ_standard_answer_handler(struct russ_conn *);

/* helpers.c */
struct russ_conn *russ_execv(russ_deadline, char *, char **, char **);
struct russ_conn *russ_execl(russ_deadline, char *, char **, ...);
struct russ_conn *russ_help(russ_deadline, char *);
struct russ_conn *russ_info(russ_deadline, char *);
struct russ_conn *russ_list(russ_deadline, char *);

/* io.c */
int russ_close(int);
ssize_t russ_read(int, char *, size_t);
ssize_t russ_readline(int, char *, size_t);
ssize_t russ_readn(int, char *, size_t);
ssize_t russ_readn_deadline(int, char *, size_t, russ_deadline);
ssize_t russ_writen(int, char *, size_t);
ssize_t russ_writen_deadline(int, char *, size_t, russ_deadline);

/* listener.c */
struct russ_lis *russ_announce(char *, mode_t, uid_t, gid_t);
struct russ_conn *russ_lis_accept(struct russ_lis *, russ_deadline);
void russ_lis_close(struct russ_lis *);
struct russ_lis *russ_lis_free(struct russ_lis *);
void russ_lis_loop(struct russ_lis *, russ_accepthandler, russ_answerhandler, russ_reqhandler);

/* misc.c */
int russ_dprintf(int, char *, ...);
int russ_misc_str_count(char *, char *);
int russ_sarray0_count(char **, int);
char **russ_sarray0_dup(char **, int);
void *russ_malloc(size_t);
const char *russ_op_lookup(russ_opnum);
russ_opnum russ_opnum_lookup(char *);
int russ_switch_user(uid_t, gid_t, int, gid_t *);
int russ_unlink(char *);

/* request.c */

/* server.c */
struct russ_svr *russ_svr_new(struct russ_svcnode *, int);
struct russ_conn *russ_svr_accept(struct russ_svr *, russ_deadline);
struct russ_lis *russ_svr_announce(struct russ_svr *, char *, mode_t, uid_t, gid_t);
void russ_svr_loop(struct russ_svr *);
int russ_svr_set_accepthandler(struct russ_svr *, russ_accepthandler);
int russ_svr_set_auto_switch_user(struct russ_svr *, int);

/* servicenodes.c */
struct russ_svcnode *russ_svcnode_new(char *, russ_svchandler);
struct russ_svcnode *russ_svcnode_free(struct russ_svcnode *);
struct russ_svcnode *russ_svcnode_add(struct russ_svcnode *, char *, russ_svchandler);
struct russ_svcnode *russ_svcnode_find(struct russ_svcnode *, char *, char *, int);
int russ_svcnode_set_auto_answer(struct russ_svcnode *, int);
int russ_svcnode_set_virtual(struct russ_svcnode *, int);

/* spath.c */
int russ_spath_split(char *, char **, char **);
char *russ_spath_resolve(char *);
char *russ_spath_resolve_with_uid(char *, uid_t *);

/* time.c */
russ_deadline russ_gettime(void); /* internal */
russ_deadline russ_to_deadline(int);
russ_deadline russ_to_deadline_diff(russ_deadline);
int russ_to_timeout(russ_deadline deadline);

#endif /* RUSS_H */
