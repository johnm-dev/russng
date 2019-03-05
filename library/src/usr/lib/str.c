/*
* lib/str.c
*/

/*
# license--start
#
# Copyright 2012-2013 John Marshall
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
#include <stdlib.h>
#include <string.h>

/* prototypes */
char *russ_sarray0_get_suffix(char **, char *);
void *russ_free(void *);

/**
* Count the number of substrings in the string.
*
* @param s		string
* @param ss		substring
* @return		# of instances found
*/
int
russ_str_count_sub(const char *s, const char *ss) {
	int	ss_len, n;

	ss_len = strlen(ss);
	for (n = 0; s != NULL; n++) {
		if ((s = strstr(s, ss)) == NULL) {
			break;
		}
		s += ss_len;
	}
	return n;
}

/**
* Find the idx-th component from the string as divided by a
* separator and return a duplicate of it (which must be freed by the
* caller).
*
* If string starts with separator, then component at index 0 returns
* "" (empty string).
*
* @param s		string
* @param sep		separator character
* @param idx		index of the desired component
* @return		copy of component string; NULL on error/unavailable
*/
char *
russ_str_dup_comp(const char *s, char sep, int idx) {
	char	*p = NULL;

	for (; idx > 0; idx--) {
		if ((s = strchr(s, sep)) == NULL) {
			return NULL;
		}
		s++;
	}
	if ((p = strchr(s, sep)) == NULL) {
		s = strdup(s);
	} else {
		s = strndup(s, p-s);
	}
	return (char *)s;
}

/**
* Like russ_str_dup_comp() except the component is copied into a
* supplied (and known-sized) buffer. The buffer is safely null-
* terminated.
*
* @param s		string
* @param sep		separator character
* @param idx		index of the desired component
* @param b		buffer
* @param sz		buffer size
* @return		0 on success; -1 on failure
*/
int
russ_str_get_comp(const char *s, char sep, int idx, char *b, int sz) {
	char	*p = NULL;

	for(; idx > 0; idx--) {
		if ((s = strchr(s, sep)) == NULL) {
			return -1;
		}
		s++;
	}
	if ((p = strchr(s, sep)) == NULL) {
		p = (char *)s+strlen(s);
	}
	if ((p-s > sz-1)
		|| (strncpy(b, s, p-s) == NULL)) {
		return -1;
	}
	b[p-s] = '\0';
	return 0;
}

/**
* Replace character in string.
*
* @param s		string
* @param oldch		old character
* @param newch		new character
* @return		string
*/
char *
russ_str_replace_char(char *s, char oldch, char newch) {
	if (s != NULL) {
		for (; *s != '\0'; s++) {
			if (*s == oldch) {
				*s = newch;
			}
		}
	}
	return s;
}

/**
* Resolve a format string using a NULL-terminated list of strings of
* the form name=value, and return a new string.
*
* Resolving is done for references in a string with the format
* ${name}. If the name exists in the list, its value replaces the
* ${name} string; otherwise an empty string is used.
*
* A failure occurs *only* if the size of the working space is
* exceeded.
*
* The return value must be freed by the caller.
*
* @param		s
* @return		resolved string; NULL on failure
*/
char *
russ_str_resolve(const char *s, char **vars) {
	const char	*start = NULL, *end = NULL;
	const char	*sp = NULL, *spend = NULL;
	char		*fp = NULL, *fpend = NULL;
	char		*value = NULL;
	int		valuelen;
	char		final[16000], name[256], prefix[256];

	sp = s;
	spend = s+strlen(s)+1;
	fp = final;
	fpend = final+sizeof(final);
	final[0] = '\0';

	while (sp < spend) {
		if ((start = strstr(sp, "${")) == NULL) {
			// nothing left to resolve
			break;
		}

		// leading part
		if (start > sp) {
			if (start-sp > fpend-fp) {
				return NULL;
			}
			strncpy(fp, sp, start-sp);
			fp += start-sp;
		}

		// to resolve part
		start += 2;
		if ((end = strchr(start+1, '}')) == NULL) {
			// cannot find terminator
			return NULL;
		}
		if (end-start+1 > sizeof(name)) {
			// name too long
			return NULL;
		}
		strncpy(name, start, end-start);
		name[end-start] = '\0';

		strcpy(prefix, name);
		prefix[end-start] = '=';
		prefix[end-start+1] = '\0';

		if ((value = russ_sarray0_get_suffix(vars, prefix)) != NULL) {
			valuelen = strlen(value);
			if (fp+valuelen+1 > fpend) {
				value = russ_free(value);
				return NULL;
			}
			strcpy(fp, value);
			fp += valuelen;
			//value = russ_free(value);
		}
		sp = end+1;
	}

	// trailing part
	if (spend-sp > fpend-fp) {
		return NULL;
	}
	strncpy(fp, sp, spend-sp);
	fp += spend-sp;

	return strdup(final);
}
