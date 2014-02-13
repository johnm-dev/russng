/*
* include/russ_priv.h
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

#ifndef RUSS_PRIV_H
#define RUSS_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <poll.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "russ.h"

#define RUSS_REQ_BUF_MAX	262144
#define RUSS_LISTEN_BACKLOG	1024

/* conn.c */
struct russ_conn *russ_conn_new(void);
int russ_conn_send_request(struct russ_conn *, russ_deadline, struct russ_req *);

/* encdec.c */
char *russ_dec_H(char *, uint16_t *);
char *russ_dec_I(char *, uint32_t *);
char *russ_dec_i(char *, int32_t *);
char *russ_dec_Q(char *, uint64_t *);
char *russ_dec_q(char *, int64_t *);
char *russ_dec_b(char *, char **);
char *russ_dec_s(char *, char **);
char *russ_dec_sarray0(char *, char ***, int *);
char *russ_dec_sarrayn(char *, char ***, int *);
char *russ_dec_exit(char *, int *);

char *russ_enc_H(char *, char *, uint16_t);
char *russ_enc_I(char *, char *, uint32_t);
char *russ_enc_i(char *, char *, int32_t);
char *russ_enc_Q(char *, char *, uint64_t);
char *russ_enc_q(char *, char *, int64_t);
char *russ_enc_b(char *, char *, char *, int);
char *russ_enc_s(char *, char *, char *);
char *russ_enc_sarrayn(char *, char *, char **, int);
char *russ_enc_sarray0(char *, char *, char **);
char *russ_enc_exit(char *, char *, int);

/* fd.c */
void russ_fds_init(int *, int, int);
void russ_fds_close(int *, int);
int russ_make_pipes(int, int *, int *);

/* io.c */
int russ_accept_deadline(russ_deadline, int, struct sockaddr *, socklen_t *);
int russ_connect_deadline(russ_deadline, int sd, struct sockaddr *, socklen_t);
int russ_connectunix_deadline(russ_deadline, char *);
int russ_poll_deadline(russ_deadline, struct pollfd *, int);

/* req.c */
struct russ_req *russ_req_new(const char *, const char *, const char *, char **, char **);
struct russ_req *russ_req_free(struct russ_req *);

/* socket.c */
int russ_get_creds(int, struct russ_creds *);
int russ_recvfd(int, int *);
int russ_sendfd(int, int);

#ifdef __cplusplus
}
#endif

#endif /* RUSS_PRIV_H */
