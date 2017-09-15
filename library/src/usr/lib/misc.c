/*
* lib/misc.c
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

#include "russ_priv.h"

/**
* Write an (encoded) exit status to an fd.
*
* @param fd		file descriptor (presumably the exit fd)
* @param exitst		exit status to encode and write
* @return		0 on success; -1 on failure
*
*/
int
russ_write_exit(int fd, int exitst) {
	char	buf[16];
	char	*bp = NULL;

	if (((bp = russ_enc_exit(buf, buf+sizeof(buf), exitst)) == NULL)
		|| (russ_writen(fd, buf, bp-buf) < bp-buf)) {
		return -1;
	}
	return 0;
}
