/*
* lib/buf.c
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
#include <string.h>

#include <russ/priv.h>

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
	if ((data == NULL) && (cap > 0) && ((data = russ_malloc(sizeof(char)*cap)) == NULL)) {
		return -1;
	}
	self->data = russ_free(self->data);

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
	struct russ_buf	*self = NULL;

	if (((self = russ_malloc(sizeof(struct russ_buf))) == NULL)
		|| ((self->data = russ_malloc(cap)) == NULL)) {
		goto free_buf;
	}
	self->cap = cap;
	self->len = 0;
	self->off = 0;
	return self;
free_buf:
	self = russ_free(self);
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
		self->data = russ_free(self->data);
		self = russ_free(self);
	}
	return NULL;
}

/**
* Adjust internal length. Cannot move past 0 or capacity.
*
* @param self		russ_buf object
* @param delta		number of bytes to move internal length
* @return		# of bytes of capacity available
*/
int
russ_buf_adjlen(struct russ_buf *self, int delta) {
	if (delta != 0) {
		self->len += delta;
		if (self->len < 0) {
			self->len = 0;
		} else if (self->len > self->cap) {
			self->len = self->cap;
		}
	}
	return self->cap-self->len;
}

/**
* Return pointer to next available byte.
*
* @param self		russ_buf object
* @param navail (out)	pointer to hold # of data bytes available
* @param cap (out)	pointer to hold # of data bytes capacity
* @return		pointer to next byte; NULL if none
*/
char *
russ_buf_getp(struct russ_buf *self, int *navail, int *cap) {
	if (navail != NULL) {
		*navail = self->len-self->off;
	}
	if (cap != NULL) {
		*cap = self->cap-self->off;
	}
	return self->data+self->off;
}

/**
* Reposition/move internal offset. Cannot move past 0 or length.
*
* @param self		russ_buf object
* @param delta		number of bytes to move internal offset
* @return		# of bytes available
*/
int
russ_buf_repos(struct russ_buf *self, int delta) {
	if (delta != 0) {
		self->off += delta;
		if (self->off < 0) {
			self->off = 0;
		} else if (self->off > self->len) {
			self->off = self->len;
		}
	}
	return self->len-self->off;
}

/**
* Reset buffer state to empty.
*
* @param self		russ_buf object
*/
void
russ_buf_reset(struct russ_buf *self) {
	self->off = 0;
	self->len = 0;
}

/**
* Resize buffer capacity. If necessary, offset and length are
* adjusted to fit within the new capacity.
*
* @param self		russ_buf object
* @param newcap		new capacity
* @return		0 on success, -1 on failure
*/
int
russ_buf_resize(struct russ_buf *self, int newcap) {
	if ((self->data = realloc(self->data, newcap)) == NULL) {
		return -1;
	}
	if (self->len > newcap) {
		self->len = newcap;
	}
	if (self->off > self->len) {
		self->off = self->len;
	}
	self->cap = newcap;
	return 0;
}

/**
* Copy to internal buffer.
*
* @param self		russ_buf object
* @param buf		buf to copy from
* @param count		number of bytes to copy
* @return		number of bytes copied; -1 on failure
*/
int
russ_buf_set(struct russ_buf *self, char *buf, int count) {
	if (count > self->cap) {
		return -1;
	}
	memcpy(self->data, buf, count);
	self->off = 0;
	self->len = count;
	return count;
}
