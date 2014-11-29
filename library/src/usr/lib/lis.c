/*
* lib/lis.c
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

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "russ_priv.h"

/**
* Create a russ_lis object.
*
* @param sd		initial descriptor
* @return		russ_lis object; NULL on failure
*/
struct russ_lis *
russ_lis_new(int sd) {
	struct russ_lis	*self;

	if ((self = malloc(sizeof(struct russ_lis))) == NULL) {
		return NULL;
	}
	self->sd = sd;
	return self;
}


/**
* Close listener.
*
* @param self		listener object
*/
void
russ_lis_close(struct russ_lis *self) {
	if (self->sd > -1) {
		russ_fds_close(&self->sd, 1);
	}
}

/**
* Free listener object.
*
* @param self		listener object
* @return		NULL value
*/
struct russ_lis *
russ_lis_free(struct russ_lis *self) {
	self = russ_free(self);
	return NULL;
}
