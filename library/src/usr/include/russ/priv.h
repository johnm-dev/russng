/*
* include/russ/priv.h
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

#ifndef _RUSS_PRIV_H
#define _RUSS_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "russ/russ.h"

#define RUSS_REQ_BUF_MAX	262144
#define RUSS_LISTEN_BACKLOG	1024

/* cconn.c */
int russ_cconn_send_req(struct russ_cconn *, russ_deadline, struct russ_req *);

/* encdec.c */
char *russ_dec_uint16(char *, uint16_t *);
char *russ_dec_int16(char *, int16_t *);
char *russ_dec_uint32(char *, uint32_t *);
char *russ_dec_int32(char *, int32_t *);
char *russ_dec_uint64(char *, uint64_t *);
char *russ_dec_int64(char *, int64_t *);
char *russ_dec_bytes(char *, char **);
char *russ_dec_s(char *, char **);
char *russ_dec_sarray0(char *, char ***, int *);
char *russ_dec_sarrayn(char *, char ***, int *);
char *russ_dec_exit(char *, int *);
char *russ_dec_req(char *, struct russ_req **);

char *russ_enc_uint16(char *, char *, uint16_t);
char *russ_enc_int16(char *, char *, int16_t);
char *russ_enc_uint32(char *, char *, uint32_t);
char *russ_enc_int32(char *, char *, int32_t);
char *russ_enc_uint64(char *, char *, uint64_t);
char *russ_enc_int64(char *, char *, int64_t);
char *russ_enc_bytes(char *, char *, char *, int);
char *russ_enc_s(char *, char *, char *);
char *russ_enc_sarrayn(char *, char *, char **, int);
char *russ_enc_sarray0(char *, char *, char **);
char *russ_enc_exit(char *, char *, int);
char *russ_enc_req(char *, char *, struct russ_req *);

/* fd.c */
int russ_test_fd(int, int);
void russ_fds_init(int *, int, int);
void russ_fds_close(int *, int);
int russ_make_pipes(int, int *, int *);
int russ_poll_deadline(russ_deadline, struct pollfd *, int);

/* req.c */
struct russ_req *russ_req_new(const char *, const char *, const char *, char **, char **);
struct russ_req *russ_req_free(struct russ_req *);

/* socket.c */
int russ_accept_deadline(russ_deadline, int, struct sockaddr *, socklen_t *);
int russ_connect_deadline(russ_deadline, int, struct sockaddr *, socklen_t);
int russ_connectunix_deadline(russ_deadline, char *);
int russ_get_creds(int, struct russ_creds *);
int russ_recv_fd(int, int *);
int russ_send_fd(int, int);

/* svr-fork.c */
void russ_svr_loop_fork(struct russ_svr *);

/* svr-pthread.c */
void russ_svr_loop_thread(struct russ_svr *);

/* user.c */
gid_t russ_group2gid(char *);
uid_t russ_user2uid(char *);

#ifdef __cplusplus
}
#endif

#endif /* _RUSS_PRIV_H */
