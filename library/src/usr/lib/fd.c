/*
* lib/fd.h
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

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

/* external */
int russ_close(int);

/**
* Initialize descriptor array to value.
*
* @param self		descriptor array
* @param count		size of fds array
* @param value		initialization value
*/
void
russ_fds_init(int *self, int count, int value) {
	int	i;

	for (i = 0; i < count; i++) {
		self[i] = value;
	}
}

/**
* Close descriptors in array and set to -1.
*
* @param self		descriptor array
* @param count		size of fds array
*/
void
russ_fds_close(int *self, int count) {
	int	i;

	for (i = 0; i < count; i++) {
		if (self[i] > -1) {
			russ_close(self[i]);
			self[i] = -1;
		}
	}
}

/**
* Make pipes and store passed arrays.
*
* Pipes are created with read and write descriptors in stored to
* separate arrays. A failure releases all created pipes.
*
* @param count		# of pipes to make; minimum size of rfds and wfds
* @param[out] rfds	array for created read fds
* @param[out] wfds	array for created write fds 
* @return		0 on success; -1 on error
*/
int
russ_make_pipes(int count, int *rfds, int *wfds) {
	int	i, pfds[2];

	russ_fds_init(rfds, count, -1);
	russ_fds_init(wfds, count, -1);

	for (i = 0; i < count; i++) {
		if (count == 3) {
			if (socketpair(AF_UNIX, SOCK_STREAM, 0, pfds) < 0) {
				goto close_fds;
			}
		} else {
			if (pipe(pfds) < 0) {
				goto close_fds;
			}
		}
		rfds[i] = pfds[0];
		wfds[i] = pfds[1];
	}
	return 0;

close_fds:
	russ_fds_close(rfds, i);
	russ_fds_close(wfds, i);
	return -1;
}
