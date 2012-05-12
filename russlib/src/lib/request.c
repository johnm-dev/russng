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
* Initialize connection request. All provided (non NULL) information is duplicated.
*/
int
russ_init_request(struct russ_conn *conn, char *protocol_string, char *spath, char *op, char **attrv, char **argv) {
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
	if (attrv) {
		if ((req->attrv = russ_dup_str_array0(attrv, &(req->attrc), RUSS_MAX_ATTRC)) == NULL) {
			goto free_req_items;
		}
		/* do not count the NULL sentinel */
		req->attrc--;
	}
	if (argv) {
		if ((req->argv = russ_dup_str_array0(argv, &(req->argc), RUSS_MAX_ARGC)) == NULL) {
			goto free_req_items;
		}
		/* do not count the NULL sentinel */
		req->argc--;
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
	russ_init_request(conn, NULL, NULL, NULL, NULL, NULL);
}


/**
* Send request over conn.
*
* @param timeout	time in which to complete the send
* @param conn	connection object
* @return	0 on success, -1 on error
*/
int
russ_send_request(russ_timeout timeout, struct russ_conn *conn) {
	struct russ_request	*req;
	char			buf[MAX_REQUEST_BUF_SIZE], *bp, *bend;

	req = &(conn->req);
	bp = buf;
	bend = buf+sizeof(buf);
	if (((bp = russ_enc_i(bp, bend, 0)) == NULL)
		|| ((bp = russ_enc_string(bp, bend, req->protocol_string)) == NULL)
		|| ((bp = russ_enc_string(bp, bend, req->spath)) == NULL)
		|| ((bp = russ_enc_string(bp, bend, req->op)) == NULL)
		|| ((bp = russ_enc_s_array0(bp, bend, req->attrv)) == NULL)
		|| ((bp = russ_enc_s_array0(bp, bend, req->argv)) == NULL)) {
		//|| ((bp = russ_enc_s_arrayn(bp, bend, req->argv, req->argc)) == NULL)) {
		return -1;
	}

	/* patch size and send */
	russ_enc_i(buf, bend, bp-buf-4);
	if (russ_writen_timeout(timeout, conn->sd, buf, bp-buf) < bp-buf) {
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
	char			buf[MAX_REQUEST_BUF_SIZE], *bp;
	int			size;

	/* get request size, load, and upack */
	bp = buf;
	if ((russ_readn(conn->sd, bp, 4) < 0)
		|| ((bp = russ_dec_i(bp, &size)) == NULL)
		|| (russ_readn(conn->sd, bp, size) < 0)) {
		return -1;
	}

	req = &(conn->req);
	if (((bp = russ_dec_s(bp, &(req->protocol_string))) == NULL)
		|| (strcmp(RUSS_PROTOCOL_STRING, req->protocol_string) != 0)
		|| ((bp = russ_dec_s(bp, &(req->spath))) == NULL)
		|| ((bp = russ_dec_s(bp, &(req->op))) == NULL)
		|| ((bp = russ_dec_s_array0(bp, &(req->attrv), &(req->attrc))) == NULL)
		|| ((bp = russ_dec_s_array0(bp, &(req->argv), &(req->argc))) == NULL)) {

		goto free_req_items;
	}
	return 0;
free_req_items:
	russ_free_request_members(conn);
	return -1;
}
