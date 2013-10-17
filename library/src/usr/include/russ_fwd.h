/*
** include/russ_fwd.h
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

#ifndef RUSS_FWD_H
#define RUSS_FWD_H

/* forwarder reasons */
#define RUSS_FWD_CLOSE_IN	1
#define RUSS_FWD_CLOSE_OUT	2
#define RUSS_FWD_CLOSE_INOUT	RUSS_FWD_CLOSE_IN|RUSS_FWD_CLOSE_OUT

#define RUSS_FWD_REASON_EOF	0
#define RUSS_FWD_REASON_ERROR	-1
#define RUSS_FWD_REASON_TIMEOUT	-2
#define RUSS_FWD_REASON_COUNT	-3
#define RUSS_FWD_REASON_IN_HUP	-4
#define RUSS_FWD_REASON_OUT_HUP	-5

struct russ_fwd {
	pthread_t	th;		/**< thread doing forwarding */
	int		id;		/**< configurable id */
	int		in_fd;		/**< input fd */
	int		out_fd;		/**< output fd */
	int		count;		/**< # of bytes to forward */
	int		blocksize;	/**< max size of blocks at once */
	int		how;		/**< 0 for normal read, 1 for readline */
	int		close_fds;	/**< 1 to close fds before returning */
	int		reason;		/**< reason forwarder returned */
};

void russ_fwd_init(struct russ_fwd *, int, int, int, int, int, int, int);
int russ_fwd_start(struct russ_fwd *);
int russ_fwd_join(struct russ_fwd *);
int russ_fwds_run(struct russ_fwd *, int);

#endif /* RUSS_FWD_H */