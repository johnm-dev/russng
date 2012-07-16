/*
** lib/fd.h
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
			close(self[i]);
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
		if (pipe(pfds) < 0) {
			goto close_fds;
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
