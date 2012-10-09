/*
** lib/time.c
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

#include <limits.h>
#include <sys/types.h>

#include <time.h>
#ifndef _POSIX_MONOTONIC_CLOCK
#include <sys/time.h>
#endif

#include <unistd.h>

#include "russ_priv.h"

/**
* Get current time as deadline value. Meant for internal use.
*
* @return		deadline value
*/
inline russ_deadline
russ_gettime(void) {
#ifdef _POSIX_MONOTONIC_CLOCK
	struct timespec	tp;

	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (russ_deadline)(tp.tv_sec*1000 + (tp.tv_nsec/1000000));
#else
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (russ_deadline)(tv.tv_sec*1000 + (tv.tv_usec/1000));
#endif /* _POSIX_MONOTONIC_CLOCK */
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
russ_to_deadline_diff(russ_deadline deadline) {
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