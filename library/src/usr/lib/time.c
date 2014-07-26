/*
* lib/time.c
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

#include <limits.h>
#include <sys/types.h>
#include <unistd.h>

#if (_POSIX_TIMERS > 0)
#include <time.h>
#else
#include <sys/time.h>
#endif

#include "russ_priv.h"

/**
* Get current time as deadline value. Meant for internal use.
*
* @return		deadline value
*/
inline russ_deadline
russ_gettime(void) {
#if (_POSIX_TIMERS > 0)
	struct timespec	tp;

	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (russ_deadline)(tp.tv_sec*1000 + (tp.tv_nsec/1000000));
#else
	/* fallback */
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (russ_deadline)(tv.tv_sec*1000 + (tv.tv_usec/1000));
#endif /* _POSIX_TIMERS */
}

/**
* Compute deadline from timeout relative to the current time.
*
* @param timeout	timeout value (msec)
* @return		computed deadline value
*/
inline russ_deadline
russ_to_deadline(int timeout) {
	return russ_gettime()+timeout;
}

/**
* Compute the difference between a deadline and the current time.
*
* @param deadline	deadline value
* @return		difference (msec)
*/
inline russ_deadline
russ_to_deadlinediff(russ_deadline deadline) {
	return deadline-russ_gettime();
}

/**
* Compute timeout from deadline relative to the current time.
* Timeout is computed to be 0 <= timeout <= INT_MAX. A deadline in
* the past will return a timeout of 0.
* 
*
* @param deadline	deadline value
* @return		computed timeout value (msec)
*/
inline int
russ_to_timeout(russ_deadline deadline) {
	deadline -= russ_gettime();
	if (deadline < 0) {
		return 0;
	} else if (deadline > INT_MAX) {
		return INT_MAX;
	}
	return (int)deadline;
}
