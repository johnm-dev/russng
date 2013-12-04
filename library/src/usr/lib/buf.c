/*
** lib/buf.c
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

#include <stdlib.h>

#include <russ_priv.h>

/**
* Initialize a russ_buf object with new data. Optionally allocate
* new space for data.
*
* @param self		russ_buf object
* @param data		buffer data (NULL to create)
* @param cap		buffer capacity
* @param len		# of valid bytes
* @return		0 on success; -1 on failure
*/
int
russ_buf_init(struct russ_buf *self, char *data, int cap, int len) {
	if (self == NULL) {
		return -1;
	}
	if ((data == NULL) && (cap > 0) && ((data = malloc(sizeof(char)*cap)) == NULL)) {
		return -1;
	}
	free(self->data);

	self->data = data;
	self->cap = cap;
	self->len = len;
	self->off = 0;
	
	return 0;
}

/**
* Create a new russ_buf object.
*
* A russ_buf object has a capacity (maximum number of bytes that
* can be stored), a length (number of bytes stored), an offset
* (number of bytes from the beginning of the storage).
*
* @param cap		buffer capacity
* @return		russ_buf object; NULL on failure
*/
struct russ_buf *
russ_buf_new(int cap) {
	struct russ_buf	*self;

	if (((self = malloc(sizeof(struct russ_buf))) == NULL)
		|| ((self->data = malloc(cap)) == NULL)) {
		goto free_buf;
	}
	self->cap = cap;
	self->len = 0;
	self->off = 0;
	return self;
free_buf:
	free(self);
	return NULL;
}

/**
* Free russ_buf object (including data).
*
* @param self		russ_buf object
* @return		NULL
*/
struct russ_buf *
russ_buf_free(struct russ_buf *self) {
	if (self) {
		free(self->data);
		free(self);
	}
	return NULL;
}