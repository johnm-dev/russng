/*
** lib/request.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "russ_priv.h"

/**
* Duplicate a string array.
*
* @param src	source string array
* @param copy_cnt	# of elements copied
* @param max_cnt	max # of elements supported
* @return	duplicated array
*/
char **
dup_str_array(char **src, int *copy_cnt, int max_cnt) {
	char	**dst;
	int	i, cnt;

	if (src == NULL) {
		return NULL;
	}
	for (cnt = 0; (cnt < max_cnt) && (src[cnt] != NULL); cnt++);
	cnt++;

	if ((dst = malloc(sizeof(char *)*(cnt))) == NULL) {
		return NULL;
	}
	for (i = 0; i < cnt; i++) {
		if (src[i] == NULL) {
			dst[i] = NULL;
		} else if ((dst[i] = strdup(src[i])) == NULL) {
			goto free_dst;
		}
	}
	*copy_cnt = cnt;
	return dst;
free_dst:
	for (; i >= 0; i--) {
		free(dst[i]);
	}
	*copy_cnt = 0;
	return NULL;
}

/**
* Initialize connection request. All provided (non NULL) information is duplicated.
*/
int
russ_init_request(struct russ_conn *conn, char *protocol_string, char *spath, char *op, char **attrv, int argc, char **argv) {
	struct russ_request	*req;
	int			i;

	req = &(conn->req);
	req->protocol_string = NULL;
	req->spath = NULL;
	req->op = NULL;
	req->attrc = 0;
	req->attrv = NULL;
	req->argc = 0;
	req->argv = NULL;

	if (((protocol_string) && ((req->protocol_string = strdup(protocol_string)) == NULL))
		|| ((spath) && ((req->spath = strdup(spath)) == NULL))
		|| ((op) && ((req->op = strdup(op)) == NULL))) {
		goto free_req_items;
	}
	if (attrv && ((req->attrv = dup_str_array(attrv, &(req->attrc), MAX_ATTRC)) == NULL)) {
		goto free_req_items;
	}
	if (argv && ((req->argv = dup_str_array(argv, &(req->argc), argc+1)) == NULL)) {
		goto free_req_items;
	}
	return 0;

free_req_items:
	russ_free_request_members(conn);
	return -1;
}

void
russ_free_request_members(struct russ_conn *conn) {
	struct russ_request	*req;
	int			i;

	req = &(conn->req);
	free(req->protocol_string);
	free(req->spath);
	free(req->op);
	for (i = 0; i < req->attrc; i++) {
		free(req->attrv[i]);
	}
	free(req->attrv);
	for (i = 0; i < req->argc; i++) {
		free(req->argv[i]);
	}
	free(req->argv);
	russ_init_request(conn, NULL, NULL, NULL, NULL, 0, NULL);
}


/**
* Send request over conn.
*
* @param russ_conn	connection object
* @param timeout	time in which to complete the send
* @return	0 on success, -1 on error
*/
int
russ_send_request(struct russ_conn *conn, int timeout) {
	struct russ_request	*req;
	char			buf[16384], *bp, *buf_end;
	int			i;

	req = &(conn->req);
	bp = buf;
	buf_end = buf+sizeof(buf)-1;

	/* dummy size, send protocol, op */
	if (((bp = russ_enc_i(bp, 0, buf_end-bp)) == NULL)
		|| ((bp = russ_enc_string(bp, req->protocol_string, buf_end-bp)) == NULL)
		|| ((bp = russ_enc_string(bp, req->spath, buf_end-bp)) == NULL)
		|| ((bp = russ_enc_string(bp, req->op, buf_end-bp)) == NULL)) {
		return -1;
	}

	/* attributes */
	if ((bp = russ_enc_i(bp, req->attrc, buf_end-bp)) == NULL) {
		return -1;
	}
	for (i = 0; i < req->attrc; i++) {
		if ((bp = russ_enc_string(bp, req->attrv[i], buf_end-bp)) == NULL) {
			return -1;
		}
	}

	/* args */
	if ((bp = russ_enc_i(bp, req->argc, buf_end-bp)) == NULL) {
		return -1;
	}
	for (i = 0; i < req->argc; i++) {
		if ((bp = russ_enc_string(bp, req->argv[i], buf_end-bp)) == NULL) {
			return -1;
		}
	}

	/* patch size and send */
	russ_enc_i(buf, bp-buf-4, 4);
	if (russ_writen_timeout(conn->sd, buf, bp-buf, timeout) < bp-buf) {
		return -1;
	}
	return 0;
}

/*
** wait for the request to come; store in conn
*/
int
russ_await_request(struct russ_conn *conn) {
	struct russ_request	*req;
	char			buf[16384], *bp;
	int			attrc, argc, count, i, size;

	/* get request size */
	bp = buf;
	if (russ_readn(conn->sd, bp, 4) < 0) {
		return -1;
	}
	size = russ_dec_i(bp, &count); bp += count;

	/* load request and unpack */
	if (russ_readn(conn->sd, bp, size) < 0) {
		return -1;
	}
	req = &(conn->req);
	req->protocol_string = russ_dec_s(bp, &count); bp += count;
	req->spath = russ_dec_s(bp, &count); bp += count;
	req->op = russ_dec_s(bp, &count); bp += count;

	attrc = russ_dec_i(bp, &count); bp += count;
	if ((req->argv = malloc(sizeof(char *)*attrc)) == NULL) {
		goto free_req_items;
	}
	for (i = 0; i < attrc; i++) {
		if ((req->attrv[i] = russ_dec_s(bp, &count)) == NULL) {
			req->attrc = i;
			goto free_req_items;
		}
		bp += count;
	}
	req->attrc = attrc;

	argc = russ_dec_i(bp, &count); bp += count;
	if ((req->argv = malloc(sizeof(char *)*argc)) == NULL) {
		goto free_req_items;
	}
	for (i = 0; i < argc; i++) {
		if ((req->argv[i] = russ_dec_s(bp, &count)) == NULL) {
			req->argc = i;
			goto free_req_items;
		}
		bp += count;
	}
	req->argc = argc;
	return 0;
free_req_items:
	russ_free_request_members(conn);
	return -1;
}
