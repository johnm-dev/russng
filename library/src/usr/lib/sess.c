/*
* lib/sess.c
*/

/*
# license--start
#
# Copyright 2020 John Marshall
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

#include <stdlib.h>
#include <string.h>

#include "russ/priv.h"

/**
* Create new session.
*
* All arguments except spath are stored as references; spath is duplicated.
*
* @param svr		server object (reference)
* @param sconn		server connection object (reference)
* @param req		request object (reference)
* @param spath		matched service path
* @return		session object on success; NULL on failure
*/
struct russ_sess *
russ_sess_new(struct russ_svr *svr, struct russ_sconn *sconn, struct russ_req *req, char *spath) {
	struct russ_sess	*self = NULL;
	char			*last = NULL;

	if ((self = russ_malloc(sizeof(struct russ_sess))) == NULL) {
		return NULL;
	}
	self->svr = svr;
	self->sconn = sconn;
	self->req = req;
	self->spath = NULL;
	self->name = NULL;
	self->options = NULL;

	if ((self->spath = strdup(spath)) == NULL) {
		return NULL;
	}
	if (((last = russ_spath_getlast(self->spath)) == NULL)
		|| ((self->name = russ_spath_getname(last)) == NULL)
		|| ((self->options = russ_spath_getoptions(last)) == NULL)) {
		goto free_session;
	}

	last = russ_free(last);
	return self;

free_session:
	last = russ_free(last);
	russ_sess_free(self);
	return NULL;
}

/**
* Free session object.
*
* @param self		session object
* @return		NULL
*/
struct russ_sess *
russ_sess_free(struct russ_sess *self) {
	/* references only */
	self->svr = NULL;
	self->sconn = NULL;
	self->req = NULL;

	/* own copy */
	self->spath = russ_free(self->spath);
	self->name = russ_free(self->name);
	self->options = russ_sarray0_free(self->options);

	self = russ_free(self);
	return NULL;
}
