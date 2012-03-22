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

/* encdec.c */
char *russ_dec_H(char *, uint16_t *);
char *russ_dec_I(char *, uint32_t *);
char *russ_dec_i(char *, int32_t *);
char *russ_dec_b(char *, char **);
char *russ_dec_s(char *, char **);
char *russ_dec_s_array0(char *, char ***, int *);
char *russ_dec_s_arrayn(char *, char ***, int *);

char *russ_enc_H(char *, char *, uint16_t);
char *russ_enc_I(char *, char *, uint32_t);
char *russ_enc_i(char *, char *, int32_t);
char *russ_enc_bytes(char *, char *, char *, int);
char *russ_enc_string(char *, char *, char *);
char *russ_enc_s_arrayn(char *, char *, char **, int);
char *russ_enc_s_array0(char *, char *, char **);

/* io.c */
int russ_poll(struct pollfd *, int, int);

/* request.c */
int russ_init_request(struct russ_conn *, char *, char *, char *, char **, int, char **);
void russ_free_request_members(struct russ_conn *);
int russ_send_request(struct russ_conn *, int);

/* socket.c */
int russ_get_credentials(int, struct russ_credentials *);
int russ_recvfd(int, int *);
int russ_sendfd(int, int);

#endif /* RUSS_PRIV_H */
