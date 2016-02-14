/*
* include/russ_conf.h
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

#ifndef RUSS_CONF_H
#define RUSS_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

struct russ_confitem {
	char	*option;
	char	*value;
	union {
		long	ivalue;
		double	fvalue;
	};
};

struct russ_confsection {
	char			*name;
	struct russ_confitem	**items;
	int			len, cap;
};

struct russ_conf {
	struct russ_confsection	**sections;
	int			len, cap;
};

struct russ_conf *russ_conf_new(void);
struct russ_conf *russ_conf_free(struct russ_conf *);
struct russ_conf *russ_conf_load(int *, char **);
struct russ_conf *russ_conf_init(int *, char **);

int russ_conf_add_section(struct russ_conf *, const char *);
int russ_conf_dup_section(struct russ_conf *, const char *, const char *);
int russ_conf_has_section(struct russ_conf *, const char *);
int russ_conf_has_option(struct russ_conf *, const char *, const char *);

char *russ_conf_get(struct russ_conf *, const char *, const char *, const char *);
long russ_conf_getint(struct russ_conf *, const char *, const char *, long);
double russ_conf_getfloat(struct russ_conf *, const char *, const char *, double);
long russ_conf_getsint(struct russ_conf *, const char *, const char *, long);
char **russ_conf_options(struct russ_conf *, const char *);
int russ_conf_read(struct russ_conf *, const char *);
int russ_conf_remove_option(struct russ_conf *, const char *, const char *);
int russ_conf_remove_section(struct russ_conf *, const char *);
int russ_conf_set(struct russ_conf *, const char *, const char *, const char *);
int russ_conf_set2(struct russ_conf *, const char *, const char *, const char *);

void russ_conf_sarray0_free(char **);
char **russ_conf_sections(struct russ_conf *);
int russ_conf_write(struct russ_conf *, FILE *);

#ifdef __cplusplus
}
#endif

#endif /* RUSS_CONF_H */
