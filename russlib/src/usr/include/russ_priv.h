/*
** include/russ_priv.h
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

#ifndef RUSS_PRIV_H
#define RUSS_PRIV_H

#include <poll.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "russ.h"

#define MAX( a,b ) (((a) > (b)) ? (a) : (b))

#define MAX_REQUEST_BUF_SIZE	262144
#define FORWARD_BLOCK_SIZE	(1<<16)

/* conn.c */
struct russ_conn *russ_conn_new(void);
int russ_conn_send_request(struct russ_conn *, russ_timeout);

/* encdec.c */
char *russ_dec_H(char *, uint16_t *);
char *russ_dec_I(char *, uint32_t *);
char *russ_dec_i(char *, int32_t *);
char *russ_dec_b(char *, char **);
char *russ_dec_s(char *, char **);
char *russ_dec_sarray0(char *, char ***, int *);
char *russ_dec_sarrayn(char *, char ***, int *);

char *russ_enc_H(char *, char *, uint16_t);
char *russ_enc_I(char *, char *, uint32_t);
char *russ_enc_i(char *, char *, int32_t);
char *russ_enc_b(char *, char *, char *, int);
char *russ_enc_s(char *, char *, char *);
char *russ_enc_sarrayn(char *, char *, char **, int);
char *russ_enc_sarray0(char *, char *, char **);

/* fd.c */
void russ_fds_init(int *, int, int);
void russ_fds_close(int *, int);
int russ_make_pipes(int, int *, int *);

/* io.c */
int russ_poll(struct pollfd *, int, russ_timeout);

/* request.c */
int russ_request_init(struct russ_request *, char *, char *, char *, char **, char **);
void russ_request_free_members(struct russ_request *);

/* socket.c */
int russ_get_credentials(int, struct russ_credentials *);
int russ_recvfd(int, int *);
int russ_sendfd(int, int);

#endif /* RUSS_PRIV_H */