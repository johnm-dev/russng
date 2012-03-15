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

#define MAX_ARGC		1024
#define PROTOCOL_STRING		"0003"
#define FORWARD_BLOCK_SIZE	(1<<16)

/* encdec.c */
uint16_t russ_dec_H(char *, int *);
uint32_t russ_dec_I(char *, int *);
int32_t russ_dec_i(char *, int *);
char *russ_dec_b(char *, int *);
char *russ_dec_s(char *, int *);

char *russ_enc_H(char *, uint16_t, int);
char *russ_enc_I(char *, uint32_t, int);
char *russ_enc_i(char *, int32_t, int);
char *russ_enc_bytes(char *, char *, int, int);
char *russ_enc_string(char *, char *, int);

/* io.c */
int russ_poll(struct pollfd *, int, int);

/* request.c */
struct russ_request *russ_new_request(char *, char *, char *, int, char **);
int russ_free_request(struct russ_request *);
int russ_send_request(struct russ_conn *, struct russ_request *, int);

/* socket.c */
int russ_get_credentials(int, struct russ_credentials *);
int russ_recvfd(int, int *);
int russ_sendfd(int, int);

#endif /* RUSS_PRIV_H */
