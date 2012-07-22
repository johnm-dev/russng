/*
* confparser.c
*
* The following are closely modelled after the Python ConfigParser
* including much of the terminology.
*
* Internal objects are never returned so as to avoid bad references
* because of (re)allocation/deallocation.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "russ_confparser.h"

/*
** item
*/

/**
* Free item object.
*
* @param self		item object
*/
static void
__russ_confparser_item_free(struct russ_confparser_item *self) {
	free(self->option);
	free(self->value);
	free(self);
}

/**
* Create item object.
*
* @param option		name
* @param value		value
* @return		item object; NULL on failure
*/
static struct russ_confparser_item *
__russ_confparser_item_new(char *option, char *value) {
	struct russ_confparser_item	*self;

	if ((self = malloc(sizeof(struct russ_confparser_item))) == NULL) {
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
	__russ_confparser_item_free(self);
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
static struct russ_confparser_section *
__russ_confparser_section_new(char *section_name) {
	struct russ_confparser_section	*self;

	if ((self = malloc(sizeof(struct russ_confparser_section))) == NULL) {
		return NULL;
	}
	self->name = NULL;
	self->items = NULL;
	self->len = 0;
	self->cap = 10;
	if (((self->name = strdup(section_name)) == NULL)
		|| ((self->items = malloc(sizeof(struct russ_confparser_item *)*self->cap)) == NULL)) {
		goto free_all;
	}
	return self;
free_all:
	free(self->name);
	free(self);
	return NULL;
}

/**
* Free section object.
*
* @param self		section object
*/
static void
__russ_confparser_section_free(struct russ_confparser_section *self) {
	int	i;

	for (i = 0; i < self->len; i++) {
		__russ_confparser_item_free(self->items[i]);
	}
	free(self->name);
	free(self->items);
	free(self);
}

/**
* Find item position in section items array.
*
* @param self		section object
* @param option		option name
* @return		index into items array; -1 if not found
*/
static int
__russ_confparser_section_find_item_pos(struct russ_confparser_section *self, char *option) {
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
static struct russ_confparser_item *
__russ_confparser_section_find_item(struct russ_confparser_section *self, char *option) {
	int	i;
	if ((i = __russ_confparser_section_find_item_pos(self, option)) < 0) {
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
static struct russ_confparser_item *
__russ_confparser_section_set(struct russ_confparser_section *self, char *option, char *value) {
	struct russ_confparser_item	**items, *item;
	int				item_pos;

	if ((item = __russ_confparser_item_new(option, value)) == NULL) {
		return NULL;
	}

	item_pos = __russ_confparser_section_find_item_pos(self, option);
	if (item_pos < 0) {
		/* add */
		if (self->len == self->cap) {
			/* resize */
			if ((items = realloc(self->items, sizeof(struct russ_confparser_item *)*(self->cap+10))) == NULL) {
				goto free_item;
			}
			self->items = items;
			self->cap += 10;
		}
		self->items[self->len] = item;
		self->len++;
	} else {
		/* replace */
		__russ_confparser_item_free(self->items[item_pos]);
		self->items[item_pos] = item;
	}
	return item;
free_item:
	__russ_confparser_item_free(item);
	return NULL;
}

/*
** russ_confparser
*/

/**
* Create russ_confparser object. Modelled after the Python
* ConfigParser class.
*
* @return		russ_confparser object; NULL on failure
*/
struct russ_confparser *
russ_confparser_new(void) {
	struct russ_confparser	*self;

	if ((self = malloc(sizeof(struct russ_confparser))) == NULL) {
		return NULL;
	}
	self->len = 0;
	self->cap = 10;
	if ((self->sections = malloc(sizeof(struct russ_confparser_section *)*self->cap)) == NULL) {
		goto free_all;
	}
	return self;
free_all:
	free(self);
	return NULL;
}

/**
* Free russ_confparser object (and all contents).
*
* @param self		russ_confparser object
*/
void
russ_confparser_free(struct russ_confparser *self) {
	int	i;

	for (i = 0; i < self->len; i++) {
		__russ_confparser_section_free(self->sections[i]);
	}
	free(self->sections);
	free(self);
}

/**
* Find section index in russ_confparser sections array.
*
* @param self		russ_confparser object
* @param section_name	section name
* @return		index into sections array; -1 if not found
*/
static int
__russ_confparser_find_section_pos(struct russ_confparser *self, char *section_name) {
	int	i;

	for (i = 0; i < self->len; i++) {
		if (strcmp(self->sections[i]->name, section_name) == 0) {
			return i;
		}
	}
	return -1;
}

/**
* Find section object in russ_confparser sections array.
*
* @param self		russ_confparser object
* @param section_name	section name
* @return		section object
*/
static struct russ_confparser_section *
__russ_confparser_find_section(struct russ_confparser *self, char *section_name) {
	int	pos;

	if ((pos = __russ_confparser_find_section_pos(self, section_name)) < 0) {
		return NULL;
	}
	return self->sections[pos];
}

/**
* Get item object by section and option name.
*
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @return		item object
*/
static struct russ_confparser_item *
__russ_confparser_get_item(struct russ_confparser *self, char *section_name, char *option) {
	struct russ_confparser_section	*section;
	struct russ_confparser_item	*item;

	if (((section = __russ_confparser_find_section(self, section_name)) == NULL)
		|| ((item = __russ_confparser_section_find_item(section, option)) == NULL)) {
		return NULL;
	}
	return item;
}

/**
* Add section to sections array.
*
* @param self		russ_confparser object
* @param section_name	section name
* @return		index into sections array; -1 on failure
*/
int
russ_confparser_add_section(struct russ_confparser *self, char *section_name) {
	struct russ_confparser_section	*section, **sections;

	if ((section = __russ_confparser_find_section(self, section_name)) != NULL) {
		/* exists */
		return -1;
	}
	if (self->len == self->cap) {
		if ((sections = realloc(self->sections, sizeof(struct russ_confparser_section *)*(self->cap+10))) == NULL) {
			return -1;
		}
		self->sections = sections;
		self->cap += 10;
	}
	if ((section = __russ_confparser_section_new(section_name)) == NULL) {
		return -1;
	}
	self->sections[self->len] = section;
	self->len++;
	return 0;
}

/**
* Test that section exists.
*
* @param self		russ_confparser object
* @param section_name	section name
* @return		0 if not found; 1 if found
*/
int
russ_confparser_has_section(struct russ_confparser *self, char *section_name) {
	if (__russ_confparser_find_section_pos(self, section_name) < 0) {
		return 0;
	}
	return 1;
}

/**
* Test that option exists.
*
* @param self		russ_confparser object
* @param section_name	section name
* @return		0 if not found; 1 if found
*/
int
russ_confparser_has_option(struct russ_confparser *self, char *section_name, char *option) {
	if (__russ_confparser_get_item(self, section_name, option) == NULL) {
		return 0;
	}
	return 1;
}

/**
* Remove option.
*
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @return		0 on success; -1 on failure
*/
int
russ_confparser_remove_option(struct russ_confparser *self, char *section_name, char *option) {
	struct russ_confparser_section	*section;
	int				pos;

	if ((section = __russ_confparser_find_section(self, section_name)) == NULL) {
		return -1;
	}
	if ((pos = __russ_confparser_section_find_item_pos(section, option)) < 0) {
		return -1;
	}
	__russ_confparser_item_free(section->items[pos]);
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
* @param self		russ_confparser object
* @param section_name	section name
* @return		0 on success; -1 on failure
*/
int
russ_confparser_remove_section(struct russ_confparser *self, char *section_name) {
	struct russ_confparser_section	*section;
	int				pos;

	if ((pos = __russ_confparser_find_section_pos(self, section_name)) < 0) {
		return -1;
	}
	__russ_confparser_section_free(self->sections[pos]);
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
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value or NULL
* @return		copy of item value; copy of dvalue if not found or NULL
*/
char *
russ_confparser_get(struct russ_confparser *self, char *section_name, char *option, char *dvalue) {
	struct russ_confparser_item	*item;

	if ((item = __russ_confparser_get_item(self, section_name, option)) == NULL) {
		if (dvalue == NULL) {
			return NULL;
		} else {
			return strdup(dvalue);
		}
	}
	return item->value;
}

/**
* Get item value as long-size integer for section name and option or
* default value if not found.
*
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
long
russ_confparser_getint(struct russ_confparser *self, char *section_name, char *option, long dvalue) {
	struct russ_confparser_item	*item;
	long				value;

	if (((item = __russ_confparser_get_item(self, section_name, option)) == NULL)
		|| (sscanf(item->value, "%ld", &value) == 0)) {
		return dvalue;
	}
	return value;
}

/**
* Get item value as double-size float for section name and option or
* default value if not found.
*
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
double
russ_confparser_getfloat(struct russ_confparser *self, char *section_name, char *option, double dvalue) {
	struct russ_confparser_item	*item;
	double				value;

	if (((item = __russ_confparser_get_item(self, section_name, option)) == NULL)
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
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
long
russ_confparser_getsint(struct russ_confparser *self, char *section_name, char *option, long dvalue) {
	struct russ_confparser_item	*item;
	char				*fmt;
	long				value;

	if ((item = __russ_confparser_get_item(self, section_name, option)) == NULL) {
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
* results must be freed (see russ_confparser_sarray0_free()).
*
* @param self		russ_confparser object
* @return		NULL-terminated array of strings; NULL on failure
*/
char **
russ_confparser_options(struct russ_confparser *self, char *section_name) {
	struct russ_confparser_section	*section;
	char				**sarray0;
	int				i;

	if (((section = __russ_confparser_find_section(self, section_name)) == NULL) 
		|| ((sarray0 = malloc(sizeof(char *)*(section->len+1))) == NULL)) {
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
	russ_confparser_sarray0_free(sarray0);
	return NULL;
}

/**
* Read settings from named file. Comments, empty lines, ordering is
* not saved.
*
* @param filename		file name
* @return			russ_confparser object; NULL on failure
*/
struct russ_confparser *
russ_confparser_read(char *filename) {
	struct russ_confparser		*cp;
	struct russ_confparser_section	*section;
	FILE				*fp;
	char				*section_name;
	char				buf[4096], *p0, *p1;

	if (((fp = fopen(filename, "r")) == NULL)
		|| ((cp = russ_confparser_new()) == NULL)) {
		return NULL;
	}
	if ((russ_confparser_add_section(cp, "DEFAULT")) < 0) {
		goto free_all;
	}
	section = __russ_confparser_find_section(cp, "DEFAULT");

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
			if ((section = __russ_confparser_find_section(cp, p0)) == NULL) {
				 if (russ_confparser_add_section(cp, p0) < 0) {
				 	goto free_all;
				}
				section = __russ_confparser_find_section(cp, p0);
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
			if (__russ_confparser_section_set(section, p0, p1) == NULL) {
				goto free_all;
			}
		}
	}
	fclose(fp);
	return cp;
free_all:
	fclose(fp);
	russ_confparser_free(cp);
	return NULL;
}

/**
* Free NULL-terminated string array.
*
* @param sarray0	NULL-terminated string array
*/
void
russ_confparser_sarray0_free(char **sarray0) {
	char	**p;

	if (sarray0) {
		for (p = sarray0; *p != NULL; p++) {
			free(*p);
		}
		free(sarray0);
	}
}

/**
* Get NULL-terminated string array of section names. The results
* must be freed (see russ_confparser_sarray0_free()).
*
* @param self		russ_confparser object
* @return		NULL-terminated array of strings; NULL on failure
*/
char **
russ_confparser_sections(struct russ_confparser *self) {
	char	**sarray0;
	int	i;

	if ((sarray0 = malloc(sizeof(char *)*(self->len+1))) == NULL) {
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
	russ_confparser_sarray0_free(sarray0);
	return NULL;
}

/**
* Set option (name and value) for existing section.
*
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @param value		option value
* @return		0 on success; -1 on failure
*/
int
russ_confparser_set(struct russ_confparser *self, char *section_name, char *option, char *value) {
	struct russ_confparser_section	*section;

	if (((section = __russ_confparser_find_section(self, section_name)) == NULL)
		|| (__russ_confparser_section_set(section, option, value) == NULL)) {
		return -1;
	}
	return 0;
}

/**
* Set option (name and value) for a new or existing section.
*
* @param self		russ_confparser object
* @param section_name	section name
* @param option		option name
* @param value		option value
* @return		0 on success; -1 on failure
*/
int
russ_confparser_set2(struct russ_confparser *self, char *section_name, char *option, char *value) {
	struct russ_confparser_section	*section;

	if ((!russ_confparser_has_section(self, section_name))
		&& (russ_confparser_add_section(self, section_name) < 0)) {
		return -1;
	}
	if (((section = __russ_confparser_find_section(self, section_name)) == NULL)
		|| (__russ_confparser_section_set(section, option, value) == NULL)) {
		return -1;
	}
	return 0;
}

/**
* Write russ_confparser contents to file. Can be read in using
* russ_confparser_read().
*
* @param self		russ_confparser object
* @param fp		FILE * object
* @return		0 on success; -1 on failure
*/
int
russ_confparser_write(struct russ_confparser *self, FILE *fp) {
	struct russ_confparser_section	**sections, *section;
	struct russ_confparser_item	**items, *item;
	int				i, j;

	for (i = 0; i < self->len; i++) {
		section = self->sections[i];
		fprintf(fp, "[%s]\n", section->name, section->len, section->cap);
		for (j = 0; j < section->len; j++) {
			item = section->items[j];
			fprintf(fp, "%s=%s\n", item->option, item->value);
		}
		printf("\n");
	}
	return 0;
}
