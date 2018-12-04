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
#include <sys/wait.h>

#include "russ/priv.h"

/**
* Wait for change in pid status or fd status.
*
* To improve responsiveness, a much smaller timeout value is used
* initially, then increased over time to reach the given timeout.
* This allows for the exit value of short run child processes to be
* returned very quickly.
*
* WARNING: not for general use.
*
* @param pid		process id
* @param status		pointer to status variable
* @param fd		file descriptor to monitor
* @param timeout	poll timeout for checking fd (250 is
*			generally recommended)
* @return		__RUSS_WAITPIDFD_PID if pid change,
*			__RUSS_WAITPIDFD_FD if fd change; -1 on
*			failure
*/
int
__russ_waitpidfd(pid_t pid, int *status, int fd, int timeout) {
	struct pollfd	pollfds[1];
	int		rv;
	int		_timeout, elapsed;

	pollfds[0].fd = fd;
	pollfds[0].events = POLLHUP;

	*status = 0;

	elapsed = 0;
	_timeout = RUSS__MIN(10, timeout);
	while (1) {
		if ((rv = waitpid(pid, status, WNOHANG)) != 0) {
			return __RUSS_WAITPIDFD_PID;
		}
		if ((rv = russ_poll_deadline(russ_to_deadline(_timeout), pollfds, 1)) != 0) {
			return __RUSS_WAITPIDFD_FD;
		}

		if (timeout != _timeout) {
			elapsed += _timeout;
			if (elapsed > 30000) {
				_timeout = timeout;
			} else if (elapsed > 10000) {
				_timeout = 100;
			} else if (elapsed > 5000) {
				_timeout = 50;
			} else if (elapsed > 2000) {
				_timeout = 20;
			} else if (elapsed > 1000) {
				_timeout = 10;
			}
			_timeout = RUSS__MIN(_timeout, timeout);
		}
	}
}

/**
* Return reference to services directory path.
*
* @return		reference (dot not free) to services
*			directory path; NULL on failure
*/
char *
russ_get_services_dir(void) {
	char	*path;

	if ((path = getenv("RUSS_SERVICES_DIR")) == NULL) {
		path = RUSS_SERVICES_DIR;
	}
	return path;
}

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
