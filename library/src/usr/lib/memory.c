/*
* lib/memory.c
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

/**
* Free memory _and_ return NULL.
*
* This consolidates into one call the good practice of resetting
* a pointer to NULL after free().
*
* @param p		pointer to malloc'd memory
* @return		NULL
*/
void *
russ_free(void *p) {
	free(p);
	return NULL;
}

/**
* Wrapper for malloc to support 0-sized malloc requests
* (see AIX malloc()).
*
* @param size		number of bytes
* @return		pointer to allocated memory
*/
void *
russ_malloc(size_t size) {
	size = (size == 0) ? 1 : size;
	return malloc(size);
}
