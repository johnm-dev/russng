/*
* lib/env.c
*/

/*
# license--start
#
# Copyright 2016 John Marshall
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

#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include <russ/priv.h>

extern char	**environ;

/**
* Cross-platform function to clear environment.
*
* @return		0 on success, -1 on failure
*/
int
russ_env_clear(void) {
#if defined(__APPLE__) || defined(__FreeBSD__)
	char	name[256];
	char	*_name = NULL;
	char	*ch = NULL;
	int	pos, rv;

	if (environ == NULL) {
		return 0;
	}

	while (*environ) {
		if (ch = strchr(*environ, '=')) {
			pos = ch-*environ;
		} else {
			pos = strlen(*environ)+1;
		}
		if (pos < sizeof(name)) {
			strncpy(name, *environ, pos);
			name[pos] = '\0';
			rv = unsetenv(name);
		} else {
			_name = strndup(*environ, pos);
			rv = unsetenv(_name);
			free(_name);
		}
		if (rv) {
			return -1;
		}
	}
	return 0;
#else
	return clearenv();
#endif
}

/**
* Reset environ.
*
* Clear and set basic settings: HOME, LOGNAME, USER.
*
* @return		0 on success; -1 on failure
*/
int
russ_env_reset(void) {
	struct passwd	*pw = NULL;

	if (((pw = getpwuid(getuid())) == NULL)
		|| (russ_env_clear() < 0)
		|| (setenv("HOME", pw->pw_dir, 1) < 0)
		|| (setenv("LOGNAME", pw->pw_name, 1) < 0)
		|| (setenv("USER", pw->pw_name, 1) < 0)) {
		return -1;
	}
	return 0;
}

/**
* Resolve string using environment variables and return a new
* string.
*
* Resolving is done for environment variables references in a
* string with the format ${name}. If the named environment variable
* exists, its value replaced the ${name} string; otherwise an empty
* string is added.
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
russ_env_resolve(const char *s) {
	const char	*start = NULL, *end = NULL;
	const char	*sp = NULL, *spend = NULL;
	char		*fp = NULL, *fpend = NULL;
	char		*value = NULL;
	int		valuelen;
	char		final[16000], name[256];

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

		if ((value = getenv(name)) != NULL) {
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

/**
* Export environ-like list to environment.
*
* @param envp		list of name=value strings list environ
* @return		0 on success; -1 on failure
*/
int
russ_env_update(char **envp) {
	char	*s = NULL;

	if (envp) {
		for (; *envp; envp++) {
			if ((s = strdup(*envp)) == NULL) {
				return -1;
			}
			if (putenv(s) < 0) {
				free(s);
				return -1;
			}
		}
	}
	return 0;
}
