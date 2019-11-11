/*
* lib/debug.c
*/

/*
# license--start
#
# Copyright 2019 John Marshall
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

#define LOAD_DEBUG_VAR(n) n = getenv(#n) ? 1 : 0;

/* debugging variables */
int \
	RUSS_DEBUG__russ_start_setlimit,
	RUSS_DEBUG_russ_connect_deadline,
	RUSS_DEBUG_russ_connectunix_deadline,
	RUSS_DEBUG_russ_dialv;

/**
* Initialize debug variables.
*
* All variables are loaded based on similarly named environment variables.
*/
void
russ_debug_init(void) {
	LOAD_DEBUG_VAR(RUSS_DEBUG__russ_start_setlimit)
	LOAD_DEBUG_VAR(RUSS_DEBUG_russ_connect_deadline)
	LOAD_DEBUG_VAR(RUSS_DEBUG_russ_connectunix_deadline)
	LOAD_DEBUG_VAR(RUSS_DEBUG_russ_dialv)
}

#define russ_debug_printf(name, ...) { if (name) { fprintf(stderr, __VA_ARGS__); } }