/*
* include/russ/russ.h
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

#ifndef _RUSS_RUSS_H
#define _RUSS_RUSS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <russ/conf.h>
#include <russ/debug.h>

/*
#define DEBUG
*/
#ifdef DEBUG
#define dprintf(...)	fprintf(stderr, __VA_ARGS__)
#else
#define dprintf(...)
#endif

#define RUSS__ABS( a )		(( (a) < 0) ? -(a) : (a))
#define RUSS__MIN( a,b )	(( (a) < (b) ) ? (a) : (b))
#define RUSS__MAX( a,b )	(( (a) > (b) ) ? (a) : (b))

#define RUSS_CONN_NFDS		32
#define RUSS_CONN_STD_NFDS	3
#define RUSS_CONN_FD_STDIN	0
#define RUSS_CONN_FD_STDOUT	1
#define RUSS_CONN_FD_STDERR	2

#define RUSS_CONN_NSYSFDS	1
#define RUSS_CONN_SYSFD_EXIT	0

#define RUSS_CONN_MAX_NFDS	( RUSS__MAX(RUSS_CONN_NFDS, RUSS_CONN_NSYSFDS) )

#define RUSS_DEADLINE_NEVER	INT64_MAX

/* common exit status values */
#define RUSS_EXIT_SUCCESS	0
#define RUSS_EXIT_FAILURE	1
#define RUSS_EXIT_EXITFDCLOSED	125
#define RUSS_EXIT_CALLFAILURE	126
#define RUSS_EXIT_SYSFAILURE	127

/* common messages */
#define RUSS_MSG_BADARGS	"error: bad/missing arguments"
#define RUSS_MSG_BADCONNEVENT	"error: unexpected connection event"
#define RUSS_MSG_BADOP		"error: operation unsupported by service"
#define RUSS_MSG_BADSITUATION	"error: unexpected situation"
#define RUSS_MSG_NOACCESS	"error: insufficient privilege"
#define RUSS_MSG_NODIAL		"error: cannot dial service"
#define RUSS_MSG_NOEXIT		"error: no exit status"
#define RUSS_MSG_NOLIST		"info: list not available"
#define RUSS_MSG_NOSERVICE	"error: no service"
#define RUSS_MSG_NOSWITCHUSER	"error: service cannot switch user"
#define RUSS_MSG_UNDEFSERVICE	"warning: undefined service"
#define RUSS_MSG_BADUSER	"error: bad user"

#define RUSS_OPNUM_NOTSET	0
#define RUSS_OPNUM_EXTENSION	1
#define RUSS_OPNUM_EXECUTE	2
#define RUSS_OPNUM_HELP		3
#define RUSS_OPNUM_ID		4
#define RUSS_OPNUM_INFO		5
#define RUSS_OPNUM_LIST		6

/* request */
#define RUSS_REQ_ARGS_MAX	1024
#define RUSS_REQ_ATTRS_MAX	1024
#define RUSS_REQ_SPATH_MAX	65536
#define RUSS_REQ_PROTOCOLSTRING	"0010"

/* start */
#define RUSS_STARTTYPE_START	1
#define RUSS_STARTTYPE_SPAWN	2

/* svr */
#define RUSS_SVR_LIS_SD_DEFAULT	3
#define RUSS_SVR_TIMEOUT_ACCEPT	INT_MAX
#define RUSS_SVR_TIMEOUT_AWAIT	15000
#define RUSS_SVR_TYPE_FORK	1
#define RUSS_SVR_TYPE_THREAD	2

#define RUSS_SERVICES_DIR	"/var/run/russ/services"

#define RUSS_WAIT_UNSET		1
#define RUSS_WAIT_OK		0
#define RUSS_WAIT_FAILURE	-1
#define RUSS_WAIT_BADFD		-2
#define RUSS_WAIT_TIMEOUT	-3
#define RUSS_WAIT_HUP		-4

#define __RUSS_WAITPIDFD_FD	1
#define __RUSS_WAITPIDFD_PID	2

typedef uint32_t	russ_opnum;

/**
* Buffer object.
*/
struct russ_buf {
	char	*data;
	int	cap;
	int	len;
	int	off;
};

/**
* Client credentials object. Obtained from connection.
*/
struct russ_creds {
	long	pid;
	long	uid;
	long	gid;
};

/**
* optable object.
*/
struct russ_optable {
	const char	*str;
	russ_opnum	num;
};

/**
* Request object.
*/
struct russ_req {
	char		*protocolstring;	/**< identifies russ protocol */
	char		*op;		/**< operation string */
	russ_opnum	opnum;		/**< operation number*/
	char		*spath;		/**< service path */
	char		**attrv;	/**< NULL-terminated array of attributes (as name=value strings) */
	char		**argv;		/**< NULL-terminated array of args */
};

/**
* Client connection object.
*/
struct russ_cconn {
	int			sd;		/**< socket descriptor */
	int			fds[RUSS_CONN_NFDS];		/**< array of fds */
	int			sysfds[RUSS_CONN_NSYSFDS];	/**< array of system fds */
	int			nevbuf;		/**< number of bytes read into evbuf */
	char			evbuf[4];	/**< buffer for exit status */
};

struct russ_sconn {
	struct russ_creds	creds;		/**< credentials */
	int			sd;		/**< socket descriptor */
	int			fds[RUSS_CONN_NFDS];		/**< array of fds */
	int			sysfds[RUSS_CONN_NSYSFDS];	/**< array of system fds */
};

/* declare here, defined below */
struct russ_sess;

typedef void (*russ_svchandler)(struct russ_sess *);
typedef int64_t russ_deadline;

typedef struct russ_sconn *(*russ_accepthandler)(russ_deadline, int);
typedef int (*russ_answerhandler)(struct russ_sconn *);
typedef void (*russ_reqhandler)(struct russ_sconn *);

/**
* Service node object.
*/
struct russ_svcnode {
	russ_svchandler		handler;
	char			*name;
	struct russ_svcnode	*next;
	struct russ_svcnode	*children;
	int			autoanswer;
	int			virtual;
	int			wildcard;
};

/**
* Server object.
*/
struct russ_svr {
	struct russ_svcnode	*root;
	int			type;
	pid_t			mpid;
	long			ctime;
	char			*saddr;
	int			lisd;
	int			closeonaccept;
	russ_accepthandler	accepthandler;
	int			accepttimeout;
	int			allowrootuser;
	russ_answerhandler	answerhandler;
	int			awaittimeout;
	int			autoswitchuser;
	int			matchclientuser;
	char			*help;
};

/**
* Session object
*/
struct russ_sess {
	struct russ_svr		*svr;
	struct russ_sconn	*sconn;
	struct russ_req		*req;
	char			*spath;
	char			*name;
	char			**options;
};

/**
* Relay and support objects.
*/
struct russ_relaystream;

typedef void (*russ_relaystream_callback)(struct russ_relaystream *, int, void *);

struct russ_relaystream {
	int		rfd;		/**< read fd */
	int		wfd;		/**< write fd */
	struct russ_buf	*rbuf;		/**< output russ_buf */
	int		closeonexit;	/**< close when exit occurs */
	int		bidir;		/**< flag as bidirectional fds */
	russ_relaystream_callback	cb; /**< callback */
	void		*cbarg;		/**< callback argument */

	/* stats */
	russ_deadline	wlast;
	russ_deadline	rlast;
	unsigned long	nrbytes;
	unsigned long	nwbytes;
	unsigned long	nreads;
	unsigned long	nwrites;
};

struct russ_relay {
	int				nstreams;
	int				exitfd;
	struct russ_relaystream		**streams;
	struct pollfd			*pollfds;
};

/* buf.c */
int russ_buf_load(struct russ_buf *, char *, int, int);
int russ_buf_init(struct russ_buf *, char *, int, int);
struct russ_buf *russ_buf_new(int);
struct russ_buf *russ_buf_free(struct russ_buf *);
int russ_buf_adjlen(struct russ_buf *, int);
char *russ_buf_getp(struct russ_buf *, int *, int *);
int russ_buf_repos(struct russ_buf *, int);
void russ_buf_reset(struct russ_buf *);
int russ_buf_resize(struct russ_buf *, int);
int russ_buf_set(struct russ_buf *, char *buf, int count);

/* cconn.c */
struct russ_cconn *russ_cconn_free(struct russ_cconn *);
struct russ_cconn *russ_cconn_new(void);
void russ_cconn_close(struct russ_cconn *);
void russ_cconn_close_fd(struct russ_cconn *, int);
int russ_cconn_wait(struct russ_cconn *, russ_deadline, int *);
struct russ_cconn *russ_dialv(russ_deadline, const char *, const char *, char **, char **);
struct russ_cconn *russ_diall(russ_deadline, const char *, const char *, char **, ...);

/* convenience.c */
struct russ_cconn *russ_dialv_timeout(int, const char *, const char *, char **, char **);
int russ_dialv_wait(russ_deadline, const char *, const char *, char **, char **, int *);
int russ_dialv_wait_timeout(int, const char *, const char *, char **, char **, int *);
int russ_dialv_wait_inouterr(russ_deadline, const char *, const char *, char **, char **, int *, struct russ_buf **);
int russ_dialv_wait_inouterr_timeout(int, const char *, const char *, char **, char **, int *, struct russ_buf **);
int russ_dialv_wait_inouterr3(russ_deadline, const char *, const char *, char **, char **, int *, struct russ_buf *, struct russ_buf *, struct russ_buf *);
struct russ_svr *russ_init(struct russ_conf *);

/* env.c */
int russ_env_clear(void);
int russ_env_reset(void);
char *russ_env_resolve(const char *);
int russ_env_setdefaults();
int russ_env_update(char **);

/* fd.c */
int russ_close(int);
void russ_close_range(int, int);
ssize_t russ_read(int, void *, size_t);
ssize_t russ_readline(int, void *, size_t);
ssize_t russ_readn(int, void *, size_t);
ssize_t russ_readn_deadline(russ_deadline, int, void *, size_t);
ssize_t russ_write(int, void *, size_t);
ssize_t russ_writen(int, void *, size_t);
ssize_t russ_writen_deadline(russ_deadline, int, void *, size_t);

/* io.c */
int russ_dprintf(int, const char *, ...);
int russ_lprintf(const char *, const char *, const char *, ...);
int russ_snprintf(char *, size_t, const char *, ...);
int russ_vdprintf(int, const char *, va_list);

/* memory.c */
void *russ_free(void *);
void *russ_malloc(size_t);

/* misc.c */
int __russ_waitpidfd(pid_t, int *, int, int);
char *russ_get_services_dir(void);
char *russ_mkstemp(char *);
int russ_write_exit(int, int);

/* optable.c */
const char *russ_optable_find_op(struct russ_optable *, russ_opnum);
russ_opnum russ_optable_find_opnum(struct russ_optable *, const char *);

/* relay.c */
struct russ_relay *russ_relay_new(int);
struct russ_relay *russ_relay_free(struct russ_relay *);
int russ_relay_add(struct russ_relay *, int, int, int, int);
int russ_relay_addwithcallback(struct russ_relay *, int, int, int, int, russ_relaystream_callback, void *);
int russ_relay_add2(struct russ_relay *, int, int, int, int);
int russ_relay_find(struct russ_relay *, int, int);
int russ_relay_loop(int, int, int *, int *, int *, int *, int);
int russ_relay_remove(struct russ_relay *, int, int);
int russ_relay_poll(struct russ_relay *, int);
int russ_relay_serve(struct russ_relay *, int, int);

/* sarray0.c */
char **russ_sarray0_new(int, ...);
char **russ_sarray0_new_split(char *, char *, int);
char **russ_sarray0_free(char **);
int russ_sarray0_append(char ***, ...);
int russ_sarray0_count(char **, int);
char **russ_sarray0_dup(char **, int);
int russ_sarray0_find(char **, char *);
int russ_sarray0_find_prefix(char **, char *);
char *russ_sarray0_get_suffix(char **, char *);
int russ_sarray0_insert(char ***, int, ...);
int russ_sarray0_remove(char **, int);
int russ_sarray0_update(char ***, int, char *);

/* sconn.c */
struct russ_sconn *russ_sconn_free(struct russ_sconn *);
struct russ_sconn *russ_sconn_new(void);
struct russ_sconn *russ_sconn_accept(russ_deadline, int);
struct russ_sconn *russ_sconn_accepthandler(russ_deadline, int);
int russ_sconn_answer(struct russ_sconn *, int, int *);
int russ_sconn_answerhandler(struct russ_sconn *);
struct russ_req *russ_sconn_await_req(struct russ_sconn *, russ_deadline);
void russ_sconn_close(struct russ_sconn *);
void russ_sconn_close_fd(struct russ_sconn *, int);
int russ_sconn_exit(struct russ_sconn *, int);
int russ_sconn_fatal(struct russ_sconn *, const char *, int);
int russ_sconn_redialandsplice(struct russ_sconn *, russ_deadline, struct russ_req *);
int russ_sconn_send_fds(struct russ_sconn *, int, int *);
int russ_sconn_splice(struct russ_sconn *, struct russ_cconn *);

/* socket.c */
int russ_announce(char *, mode_t, uid_t, gid_t);
int russ_unlink(const char *);

/* spath.c */
int russ_spath_hasoption(const char *);
char *russ_spath_getlast(const char *);
char *russ_spath_getname(const char *);
char **russ_spath_getoptions(const char *);
char *russ_spath_resolve(const char *);
char *russ_spath_resolvewithuid(const char *, uid_t *, int);
int russ_spath_split(const char *, char **, char **);
char *russ_spath_stripoptions(const char *);

/* start.c */
char *russ_start(int, struct russ_conf *);

/* str.c */
int russ_str_count_sub(const char *, const char *);
char *russ_str_dup_comp(const char *, char, int);
int russ_str_get_comp(const char *, char, int, char *, int);
char *russ_str_replace_char(char *, char, char);
char *russ_str_resolve(const char *s, char **vars);

/* svcnode.c */
struct russ_svcnode *russ_svcnode_new(const char *, russ_svchandler);
struct russ_svcnode *russ_svcnode_free(struct russ_svcnode *);
struct russ_svcnode *russ_svcnode_add(struct russ_svcnode *, const char *, russ_svchandler);
struct russ_svcnode *russ_svcnode_find(struct russ_svcnode *, const char *, char *, int);
int russ_svcnode_set_autoanswer(struct russ_svcnode *, int);
int russ_svcnode_set_handler(struct russ_svcnode *, russ_svchandler);
int russ_svcnode_set_virtual(struct russ_svcnode *, int);
int russ_svcnode_set_wildcard(struct russ_svcnode *, int);

/* svr.c */
struct russ_svr *russ_svr_new(struct russ_svcnode *, int, int);
struct russ_svr *russ_svr_free(struct russ_svr *);
struct russ_sconn *russ_svr_accept(struct russ_svr *, russ_deadline);
void russ_svr_handler(struct russ_svr *, struct russ_sconn *);
void russ_svr_loop(struct russ_svr *);
int russ_svr_set_accepthandler(struct russ_svr *, russ_accepthandler);
int russ_svr_set_accepttimeout(struct russ_svr *, int);
int russ_svr_set_allowrootuser(struct russ_svr *, int);
int russ_svr_set_answerhandler(struct russ_svr *, russ_answerhandler);
int russ_svr_set_autoswitchuser(struct russ_svr *, int);
int russ_svr_set_closeonaccept(struct russ_svr *, int);
int russ_svr_set_help(struct russ_svr *, const char *);
int russ_svr_set_matchclientuser(struct russ_svr *, int);
int russ_svr_set_root(struct russ_svr *, struct russ_svcnode *);
int russ_svr_set_lisd(struct russ_svr *, int);
int russ_svr_set_type(struct russ_svr *, int);

/* time.c */
russ_deadline russ_gettime(void); /* internal */
russ_deadline russ_to_deadline(int);
russ_deadline russ_to_deadlinediff(russ_deadline);
int russ_to_timeout(russ_deadline);

/* user.c */
int russ_switch_user(uid_t, gid_t, int, gid_t *);
int russ_switch_userinitgroups(uid_t, gid_t);

#ifdef __cplusplus
}
#endif

#endif /* _RUSS_RUSS_H */
