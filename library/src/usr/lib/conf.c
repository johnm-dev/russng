/*
* lib/conf.c
*
* The following are closely modelled after the Python ConfigParser
* including much of the terminology.
*
* Internal objects are never returned so as to avoid bad references
* because of (re)allocation/deallocation.
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <russ.h>

/*
** item
*/

/**
* Free item object.
*
* @param self		item object
* @return		NULL
*/
static struct russ_confitem *
__russ_confitem_free(struct russ_confitem *self) {
	if (self) {
		self->option = russ_free(self->option);
		self->value = russ_free(self->value);
		self = russ_free(self);
	}
	return NULL;
}

/**
* Create item object.
*
* @param option		name
* @param value		value
* @return		item object; NULL on failure
*/
static struct russ_confitem *
__russ_confitem_new(const char *option, const char *value) {
	struct russ_confitem	*self;

	if ((self = russ_malloc(sizeof(struct russ_confitem))) == NULL) {
		return NULL;
	}
	self->option = NULL;
	self->value = NULL;
	if (((self->option = strdup(option)) == NULL) ||
		((self->value = strdup(value)) == NULL)) {
		goto free_all;
	}
	return self;
free_all:
	__russ_confitem_free(self);
	return NULL;
}

/*
** section
*/

/**
* Create section object.
*
* @param section_name	section name
* @return		section object; NULL on failure
*/
static struct russ_confsection *
__russ_confsection_new(const char *section_name) {
	struct russ_confsection	*self;

	if ((self = russ_malloc(sizeof(struct russ_confsection))) == NULL) {
		return NULL;
	}
	self->name = NULL;
	self->items = NULL;
	self->len = 0;
	self->cap = 10;
	if (((self->name = strdup(section_name)) == NULL)
		|| ((self->items = russ_malloc(sizeof(struct russ_confitem *)*self->cap)) == NULL)) {
		goto free_all;
	}
	return self;
free_all:
	self->name = russ_free(self->name);
	self = russ_free(self);
	return NULL;
}

/**
* Free section object.
*
* @param self		section object
* @return		NULL
*/
static struct russ_confsection *
__russ_confsection_free(struct russ_confsection *self) {
	int	i;

	if (self) {
		for (i = 0; i < self->len; i++) {
			__russ_confitem_free(self->items[i]);
		}
		self->name = russ_free(self->name);
		self->items = russ_free(self->items);
		self = russ_free(self);
	}
	return NULL;
}

/**
* Find item position in section items array.
*
* @param self		section object
* @param option		option name
* @return		index into items array; -1 if not found
*/
static int
__russ_confsection_find_item_pos(struct russ_confsection *self, const char *option) {
	int	i;
	for (i = 0; i < self->len; i++) {
		if (strcmp(self->items[i]->option, option) == 0) {
			return i;
		}
	}
	return -1;
}

/**
* Find item item in section items array.
*
* @param self		section object
* @param option		option name
* @return		item object; NULL if not found
*/
static struct russ_confitem *
__russ_confsection_find_item(struct russ_confsection *self, const char *option) {
	int	i;
	if ((i = __russ_confsection_find_item_pos(self, option)) < 0) {
		return NULL;
	}
	return self->items[i];
}

/**
* Set/add item in section items array.
*
* @param self		section object
* @param option		option name
* @param value		option value
* @retrun		item object
*/
static struct russ_confitem *
__russ_confsection_set(struct russ_confsection *self, const char *option, const char *value) {
	struct russ_confitem	**items, *item;
	int			item_pos;

	if ((item = __russ_confitem_new(option, value)) == NULL) {
		return NULL;
	}

	item_pos = __russ_confsection_find_item_pos(self, option);
	if (item_pos < 0) {
		/* add */
		if (self->len == self->cap) {
			/* resize */
			if ((items = realloc(self->items, sizeof(struct russ_confitem *)*(self->cap+10))) == NULL) {
				goto free_item;
			}
			self->items = items;
			self->cap += 10;
		}
		self->items[self->len] = item;
		self->len++;
	} else {
		/* replace */
		__russ_confitem_free(self->items[item_pos]);
		self->items[item_pos] = item;
	}
	return item;
free_item:
	__russ_confitem_free(item);
	return NULL;
}

/*
** russ_conf
*/

/**
* Create russ_conf object. Modelled after the Python
* ConfigParser class.
*
* @return		russ_conf object; NULL on failure
*/
struct russ_conf *
russ_conf_new(void) {
	struct russ_conf	*self;

	if ((self = russ_malloc(sizeof(struct russ_conf))) == NULL) {
		return NULL;
	}
	self->len = 0;
	self->cap = 10;
	if ((self->sections = russ_malloc(sizeof(struct russ_confsection *)*self->cap)) == NULL) {
		goto free_all;
	}
	return self;
free_all:
	self = russ_free(self);
	return NULL;
}

/**
* Free russ_conf object (and all contents).
*
* @param self		russ_conf object
* @return		NULL
*/
struct russ_conf *
russ_conf_free(struct russ_conf *self) {
	int	i;

	if (self) {
		for (i = 0; i < self->len; i++) {
			__russ_confsection_free(self->sections[i]);
		}
		self->sections = russ_free(self->sections);
		self = russ_free(self);
	}
	return NULL;
}

/**
* Load conf object based on command line arguments.
*
* To help provide a standard command-line usage for russ servers,
* the command line args (argc, argv) are passed to this function
* which looks for:
* -f <filename>
* -c <section>:<option>=<value>
* -- terminates the processing
*
* The argc and argv are updated at return time; unused args are
* left in argv.
*
* @param argc		pointer to argc
* @param argv		NULL-terminated string argument list
* @return		russ_conf object; NULL on failure
*/
struct russ_conf *
russ_conf_load(int *argc, char **argv) {
	struct russ_conf	*self;
	int			i, j;
	char			*colonp, *equalp;

	if ((self = russ_conf_new()) == NULL) {
		return NULL;
	}

	for (i = 1; i < *argc; i++) {
		if ((strcmp(argv[i], "-c") == 0) && (i+1 < *argc)) {
			i++;
			if (((colonp = strchr(argv[i], ':')) == NULL)
				|| ((equalp = strchr(colonp, '=')) == NULL)) {
				goto bad_args;
			}
			*colonp = '\0';
			*equalp = '\0';
			if (russ_conf_set2(self, argv[i], colonp+1, equalp+1) < 0) {
				*colonp = ':';
				*equalp = '=';
				goto bad_args;
			}
		} else if ((strcmp(argv[i], "-f") == 0) && (i+1 < *argc)) {
			i++;
			if (russ_conf_read(self, argv[i]) < 0) {
				goto bad_args;
			}
		} else if (strcmp(argv[i], "--") == 0) {
			i++;
			break;
		} else {
			goto bad_args;
		}
	}

	/* (RUSSNG-687) temporary support to copy "server" to "main" */
	if (!russ_conf_has_section(self, "main")
		&& russ_conf_has_section(self, "server")) {
		if (russ_conf_dup_section(self, "server", "main") < 0) {
			goto free_conf;
		}
	}

	/* move arguments to front */
	for (j = 1; i < *argc; i++, j++) {
		argv[j] = argv[i];
	}
	argv[j] = NULL;
	*argc = j;

	return self;
bad_args:
	fprintf(stderr, "error: bad/missing arguments\n");
free_conf:
	russ_conf_free(self);
	return NULL;
}

struct russ_conf *
russ_conf_init(int *argc, char **argv) {
	return russ_conf_load(argc, argv);
}

/**
* Find section index in russ_conf sections array.
*
* @param self		russ_conf object
* @param section_name	section name
* @return		index into sections array; -1 if not found
*/
static int
__russ_conf_find_section_pos(struct russ_conf *self, const char *section_name) {
	int	i;

	for (i = 0; i < self->len; i++) {
		if (strcmp(self->sections[i]->name, section_name) == 0) {
			return i;
		}
	}
	return -1;
}

/**
* Find section object in russ_conf sections array.
*
* @param self		russ_conf object
* @param section_name	section name
* @return		section object
*/
static struct russ_confsection *
__russ_conf_find_section(struct russ_conf *self, const char *section_name) {
	int	pos;

	if ((pos = __russ_conf_find_section_pos(self, section_name)) < 0) {
		return NULL;
	}
	return self->sections[pos];
}

/**
* Get item object by section and option name.
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @return		item object
*/
static struct russ_confitem *
__russ_conf_get_item(struct russ_conf *self, const char *section_name, const char *option) {
	struct russ_confsection	*section;
	struct russ_confitem	*item;

	if (((section = __russ_conf_find_section(self, section_name)) == NULL)
		|| ((item = __russ_confsection_find_item(section, option)) == NULL)) {
		return NULL;
	}
	return item;
}

/**
* Add section to sections array.
*
* @param self		russ_conf object
* @param section_name	section name
* @return		index into sections array; -1 on failure
*/
int
russ_conf_add_section(struct russ_conf *self, const char *section_name) {
	struct russ_confsection	*section, **sections;

	if ((section = __russ_conf_find_section(self, section_name)) != NULL) {
		/* exists */
		return -1;
	}
	if (self->len == self->cap) {
		if ((sections = realloc(self->sections, sizeof(struct russ_confsection *)*(self->cap+10))) == NULL) {
			return -1;
		}
		self->sections = sections;
		self->cap += 10;
	}
	if ((section = __russ_confsection_new(section_name)) == NULL) {
		return -1;
	}
	self->sections[self->len] = section;
	self->len++;
	return 0;
}

/**
* Duplicate all options and values from one section to another.
* Existing items in the destination section will be overwritten.
*
* @param self		russ_conf object
* @param src_section_name
*			source section name
* @param dst_section_name
*			destination section name
* @return		0 on success; -1 on failure
*/
int
russ_conf_dup_section(struct russ_conf *self, const char *src_section_name, const char *dst_section_name) {
	struct russ_confsection	*section;
	struct russ_confitem	*item;
	int			i;

	if ((section = __russ_conf_find_section(self, src_section_name)) == NULL) {
		return 0;
	}
	if (__russ_conf_find_section(self, dst_section_name) == NULL) {
		if (russ_conf_add_section(self, dst_section_name) < 0) {
			return -1;
		}
	}
	for (i = 0; i < section->len; i++) {
		item = section->items[i];
		if (russ_conf_set(self, dst_section_name, item->option, item->value) < 0) {
			return -1;
		}
	}
	return 0;
}

/**
* Test that section exists.
*
* @param self		russ_conf object
* @param section_name	section name
* @return		0 if not found; 1 if found
*/
int
russ_conf_has_section(struct russ_conf *self, const char *section_name) {
	if (__russ_conf_find_section_pos(self, section_name) < 0) {
		return 0;
	}
	return 1;
}

/**
* Test that option exists.
*
* @param self		russ_conf object
* @param section_name	section name
* @return		0 if not found; 1 if found
*/
int
russ_conf_has_option(struct russ_conf *self, const char *section_name, const char *option) {
	if (__russ_conf_get_item(self, section_name, option) == NULL) {
		return 0;
	}
	return 1;
}

/**
* Remove option.
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @return		0 on success; -1 on failure
*/
int
russ_conf_remove_option(struct russ_conf *self, const char *section_name, const char *option) {
	struct russ_confsection	*section;
	int			pos;

	if ((section = __russ_conf_find_section(self, section_name)) == NULL) {
		return -1;
	}
	if ((pos = __russ_confsection_find_item_pos(section, option)) < 0) {
		return -1;
	}
	__russ_confitem_free(section->items[pos]);
	section->items[pos] = NULL;
	if (section->len > 1) {
		section->items[pos] = section->items[(section->len-1)];
	}
	section->len--;
	return 0;
}

/**
* Remove section.
*
* @param self		russ_conf object
* @param section_name	section name
* @return		0 on success; -1 on failure
*/
int
russ_conf_remove_section(struct russ_conf *self, const char *section_name) {
	struct russ_confsection	*section;
	int			pos;

	if ((pos = __russ_conf_find_section_pos(self, section_name)) < 0) {
		return -1;
	}
	__russ_confsection_free(self->sections[pos]);
	self->sections[pos] = NULL;
	if (self->len > 1) {
		self->sections[pos] = self->sections[(self->len-1)];
	}
	self->len--;
	return 0;
}

/**
* Get copy of item value as string for section name and option or
* copy of default value if not found. Return string must be freed
* with free().
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value or NULL
* @return		copy of item value; copy of dvalue if not found or NULL
*/
char *
russ_conf_get(struct russ_conf *self, const char *section_name, const char *option, const char *dvalue) {
	struct russ_confitem	*item;

	if ((item = __russ_conf_get_item(self, section_name, option)) == NULL) {
		if (dvalue == NULL) {
			return NULL;
		} else {
			return strdup(dvalue);
		}
	}
	return strdup(item->value);
}

/**
* Get item value as long-size integer for section name and option or
* default value if not found.
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
long
russ_conf_getint(struct russ_conf *self, const char *section_name, const char *option, long dvalue) {
	struct russ_confitem	*item;
	long			value;

	if (((item = __russ_conf_get_item(self, section_name, option)) == NULL)
		|| (sscanf(item->value, "%ld", &value) == 0)) {
		return dvalue;
	}
	return value;
}

/**
* Get item value as double-size float for section name and option or
* default value if not found.
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
double
russ_conf_getfloat(struct russ_conf *self, const char *section_name, const char *option, double dvalue) {
	struct russ_confitem	*item;
	double			value;

	if (((item = __russ_conf_get_item(self, section_name, option)) == NULL)
		|| (sscanf(item->value, "%lf", &value) == 0)) {
		return dvalue;
	}
	return value;
}

/**
* Get item value as long-size integer for section name and option or
* default value if not found. Option value determines integer type:
* 0-prefix is octal, 0x-prefix is hex, otherwise integer.
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
long
russ_conf_getsint(struct russ_conf *self, const char *section_name, const char *option, long dvalue) {
	struct russ_confitem	*item;
	char			*fmt;
	long			value;

	if ((item = __russ_conf_get_item(self, section_name, option)) == NULL) {
		return dvalue;
	}
	if (strncmp(item->value, "0x", 2) == 0) {
		fmt = "%lx";
	} else if (strncmp(item->value, "0", 1) == 0) {
		fmt = "%lo";
	} else {
		fmt = "%ld";
	}
	if (sscanf(item->value, fmt, &value) == 0) {
		return dvalue;
	}
	return value;
}

/**
* Get a NULL-terminated string array of options for a section. The
* results must be freed (see russ_conf_sarray0_free()).
*
* @param self		russ_conf object
* @return		NULL-terminated array of strings; NULL on failure
*/
char **
russ_conf_options(struct russ_conf *self, const char *section_name) {
	struct russ_confsection	*section;
	char			**sarray0;
	int			i;

	if (((section = __russ_conf_find_section(self, section_name)) == NULL) 
		|| ((sarray0 = russ_malloc(sizeof(char *)*(section->len+1))) == NULL)) {
		return NULL;
	}
	memset(sarray0, 0, sizeof(char *)*(section->len+1));
	for (i = 0; i < section->len; i++) {
		if ((sarray0[i] = strdup(section->items[i]->option)) == NULL) {
			goto free_all;
		}
	}
	return sarray0;
free_all:
	russ_conf_sarray0_free(sarray0);
	return NULL;
}

/**
* Read settings from named file. Comments, empty lines, ordering is
* not saved.
*
* @param filename		file name
* @return			0 on success; -1 on failure
*/
int
russ_conf_read(struct russ_conf *self, const char *filename) {
	struct russ_confsection	*section;
	FILE			*fp;
	char			*section_name;
	char			buf[4096], *p0, *p1;

	if ((fp = fopen(filename, "r")) == NULL) {
		return -1;
	}
	if ((russ_conf_add_section(self, "DEFAULT")) < 0) {
		goto free_all;
	}
	section = __russ_conf_find_section(self, "DEFAULT");

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* skip whitespace */
		for (p0 = buf; isspace(*p0); p0++);
		for (p1 = p0+strlen(buf)-1; isspace(*p1) && (p1 > p0); p1--) {
			*p1 = '\0';
		}

		/* process line: empty line, comment, section, option=value */
		if ((*p0 == '\0') || (*p0 == '#')) {
			/* empty line or comment */
			continue;
		} else if (*p0 == '[') {
			/* section */
			for (p0++, p1 = p0; ; p1++) {
				if (*p1 == ']') {
					*p1 = '\0';
					if (*(p1+1) != '\0') {
						/* trailing chars */
						goto free_all;
					}
					break;
				} else if  (*p1 == '\0') {
					/* bad section */
					goto free_all;
					break;
				}
			}
			if ((section = __russ_conf_find_section(self, p0)) == NULL) {
				 if (russ_conf_add_section(self, p0) < 0) {
				 	goto free_all;
				}
				section = __russ_conf_find_section(self, p0);
			}
		} else {
			/* option=value or option:value */
			for (p1 = p0; ; p1++) {
				if ((*p1 == ':') || (*p1 == '=')) {
					*p1 = '\0';
					break;
				} else if (*p1 == '\0') {
					/* bad setting */
					goto free_all;
					break;
				}
			}
			for (p1++; isspace(*p1); p1++);
			if (__russ_confsection_set(section, p0, p1) == NULL) {
				goto free_all;
			}
		}
	}
	fclose(fp);
	return 0;
free_all:
	fclose(fp);
	return -1;
}

/**
* Free NULL-terminated string array.
*
* @param sarray0	NULL-terminated string array
*/
void
russ_conf_sarray0_free(char **sarray0) {
	char	**p;

	if (sarray0) {
		for (p = sarray0; *p != NULL; p++) {
			*p = russ_free(*p);
		}
		sarray0 = russ_free(sarray0);
	}
}

/**
* Get NULL-terminated string array of section names. The results
* must be freed (see russ_conf_sarray0_free()).
*
* @param self		russ_conf object
* @return		NULL-terminated array of strings; NULL on failure
*/
char **
russ_conf_sections(struct russ_conf *self) {
	char	**sarray0;
	int	i;

	if ((sarray0 = russ_malloc(sizeof(char *)*(self->len+1))) == NULL) {
		return NULL;
	}
	memset(sarray0, 0, sizeof(char *)*(self->len+1));
	for (i = 0; i < self->len; i++) {
		if ((sarray0[i] = strdup(self->sections[i]->name)) == NULL) {
			goto free_all;
		}
	}
	return sarray0;
free_all:
	russ_conf_sarray0_free(sarray0);
	return NULL;
}

/**
* Set option (name and value) for existing section.
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @param value		option value
* @return		0 on success; -1 on failure
*/
int
russ_conf_set(struct russ_conf *self, const char *section_name, const char *option, const char *value) {
	struct russ_confsection	*section;

	if (((section = __russ_conf_find_section(self, section_name)) == NULL)
		|| (__russ_confsection_set(section, option, value) == NULL)) {
		return -1;
	}
	return 0;
}

/**
* Set option (name and value) for a new or existing section.
*
* @param self		russ_conf object
* @param section_name	section name
* @param option		option name
* @param value		option value
* @return		0 on success; -1 on failure
*/
int
russ_conf_set2(struct russ_conf *self, const char *section_name, const char *option, const char *value) {
	struct russ_confsection	*section;

	if ((!russ_conf_has_section(self, section_name))
		&& (russ_conf_add_section(self, section_name) < 0)) {
		return -1;
	}
	if (((section = __russ_conf_find_section(self, section_name)) == NULL)
		|| (__russ_confsection_set(section, option, value) == NULL)) {
		return -1;
	}
	return 0;
}

/**
* Write russ_conf contents to file. Can be read in using
* russ_conf_read().
*
* @param self		russ_conf object
* @param fp		FILE * object
* @return		0 on success; -1 on failure
*/
int
russ_conf_write(struct russ_conf *self, FILE *fp) {
	struct russ_confsection	**sections, *section;
	struct russ_confitem	**items, *item;
	int			i, j;

	for (i = 0; i < self->len; i++) {
		section = self->sections[i];
		fprintf(fp, "[%s]\n", section->name);
		for (j = 0; j < section->len; j++) {
			item = section->items[j];
			fprintf(fp, "%s=%s\n", item->option, item->value);
		}
		printf("\n");
	}
	return 0;
}
