/*
* configparser.c
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

#include "configparser.h"

/*
** item
*/

/**
* Free item object.
*
* @param self		item object
*/
static void
__configparser_item_free(struct configparser_item *self) {
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
static struct configparser_item *
__configparser_item_new(char *option, char *value) {
	struct configparser_item	*self;

	if ((self = malloc(sizeof(struct configparser_item))) == NULL) {
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
	__configparser_item_free(self);
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
static struct configparser_section *
__configparser_section_new(char *section_name) {
	struct configparser_section	*self;

	if ((self = malloc(sizeof(struct configparser_section))) == NULL) {
		return NULL;
	}
	self->name = NULL;
	self->items = NULL;
	self->len = 0;
	self->cap = 10;
	if (((self->name = strdup(section_name)) == NULL)
		|| ((self->items = malloc(sizeof(struct configparser_item *)*self->cap)) == NULL)) {
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
__configparser_section_free(struct configparser_section *self) {
	int	i;

	for (i = 0; i < self->len; i++) {
		__configparser_item_free(self->items[i]);
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
__configparser_section_find_item_pos(struct configparser_section *self, char *option) {
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
static struct configparser_item *
__configparser_section_find_item(struct configparser_section *self, char *option) {
	int	i;
	if ((i = __configparser_section_find_item_pos(self, option)) < 0) {
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
static struct configparser_item *
__configparser_section_set(struct configparser_section *self, char *option, char *value) {
	struct configparser_item	**items, *item;
	int				item_pos;

	if ((item = __configparser_item_new(option, value)) == NULL) {
		return NULL;
	}

	item_pos = __configparser_section_find_item_pos(self, option);
	if (item_pos < 0) {
		/* add */
		if (self->len == self->cap) {
			/* resize */
			if ((items = realloc(self->items, sizeof(struct configparser_item *)*(self->cap+10))) == NULL) {
				goto free_item;
			}
			self->items = items;
			self->cap += 10;
		}
		self->items[self->len] = item;
		self->len++;
	} else {
		/* replace */
		__configparser_item_free(self->items[item_pos]);
		self->items[item_pos] = item;
	}
	return item;
free_item:
	__configparser_item_free(item);
	return NULL;
}

/*
** configparser
*/

/**
* Create configparser object. Modelled after the Python
* ConfigParser class.
*
* @return		configparser object; NULL on failure
*/
struct configparser *
configparser_new(void) {
	struct configparser	*self;

	if ((self = malloc(sizeof(struct configparser))) == NULL) {
		return NULL;
	}
	self->len = 0;
	self->cap = 10;
	if ((self->sections = malloc(sizeof(struct configparser_section *)*self->cap)) == NULL) {
		goto free_all;
	}
	return self;
free_all:
	free(self);
	return NULL;
}

/**
* Free configparser object (and all contents).
*
* @param self		configparser object
*/
void
configparser_free(struct configparser *self) {
	int	i;

	for (i = 0; i < self->len; i++) {
		__configparser_section_free(self->sections[i]);
	}
	free(self->sections);
	free(self);
}

/**
* Find section index in configparser sections array.
*
* @param self		configparser object
* @param section_name	section name
* @return		index into sections array; -1 if not found
*/
static int
__configparser_find_section_pos(struct configparser *self, char *section_name) {
	int	i;

	for (i = 0; i < self->len; i++) {
		if (strcmp(self->sections[i]->name, section_name) == 0) {
			return i;
		}
	}
	return -1;
}

/**
* Find section object in configparser sections array.
*
* @param self		configparser object
* @param section_name	section name
* @return		section object
*/
static struct configparser_section *
__configparser_find_section(struct configparser *self, char *section_name) {
	int	pos;

	if ((pos = __configparser_find_section_pos(self, section_name)) < 0) {
		return NULL;
	}
	return self->sections[pos];
}

/**
* Get item object by section and option name.
*
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @return		item object
*/
static struct configparser_item *
__configparser_get_item(struct configparser *self, char *section_name, char *option) {
	struct configparser_section	*section;
	struct configparser_item	*item;

	if (((section = __configparser_find_section(self, section_name)) == NULL)
		|| ((item = __configparser_section_find_item(section, option)) == NULL)) {
		return NULL;
	}
	return item;
}

/**
* Add section to sections array.
*
* @param self		configparser object
* @param section_name	section name
* @return		index into sections array; -1 on failure
*/
int
configparser_add_section(struct configparser *self, char *section_name) {
	struct configparser_section	*section, **sections;

	if ((section = __configparser_find_section(self, section_name)) != NULL) {
		/* exists */
		return -1;
	}
	if (self->len == self->cap) {
		if ((sections = realloc(self->sections, sizeof(struct configparser_section *)*(self->cap+10))) == NULL) {
			return -1;
		}
		self->sections = sections;
		self->cap += 10;
	}
	if ((section = __configparser_section_new(section_name)) == NULL) {
		return -1;
	}
	self->sections[self->len] = section;
	self->len++;
	return 0;
}

/**
* Test that section exists.
*
* @param self		configparser object
* @param section_name	section name
* @return		0 if not found; 1 if found
*/
int
configparser_has_section(struct configparser *self, char *section_name) {
	if (__configparser_find_section_pos(self, section_name) < 0) {
		return 0;
	}
	return 1;
}

/**
* Test that option exists.
*
* @param self		configparser object
* @param section_name	section name
* @return		0 if not found; 1 if found
*/
int
configparser_has_option(struct configparser *self, char *section_name, char *option) {
	if (__configparser_get_item(self, section_name, option) == NULL) {
		return 0;
	}
	return 1;
}

/**
* Remove option.
*
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @return		0 on success; -1 on failure
*/
int
configparser_remove_option(struct configparser *self, char *section_name, char *option) {
	struct configparser_section	*section;
	int				pos;

	if ((section = __configparser_find_section(self, section_name)) == NULL) {
		return -1;
	}
	if ((pos = __configparser_section_find_item_pos(section, option)) < 0) {
		return -1;
	}
	__configparser_item_free(section->items[pos]);
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
* @param self		configparser object
* @param section_name	section name
* @return		0 on success; -1 on failure
*/
int
configparser_remove_section(struct configparser *self, char *section_name) {
	struct configparser_section	*section;
	int				pos;

	if ((pos = __configparser_find_section_pos(self, section_name)) < 0) {
		return -1;
	}
	__configparser_section_free(self->sections[pos]);
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
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value or NULL
* @return		copy of item value; copy of dvalue if not found or NULL
*/
char *
configparser_get(struct configparser *self, char *section_name, char *option, char *dvalue) {
	struct configparser_item	*item;

	if ((item = __configparser_get_item(self, section_name, option)) == NULL) {
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
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
long
configparser_getint(struct configparser *self, char *section_name, char *option, long dvalue) {
	struct configparser_item	*item;
	long				value;

	if (((item = __configparser_get_item(self, section_name, option)) == NULL)
		|| (sscanf(item->value, "%ld", &value) == 0)) {
		return dvalue;
	}
	return value;
}

/**
* Get item value as double-size float for section name and option or
* default value if not found.
*
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
double
configparser_getfloat(struct configparser *self, char *section_name, char *option, double dvalue) {
	struct configparser_item	*item;
	double				value;

	if (((item = __configparser_get_item(self, section_name, option)) == NULL)
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
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @param dvalue		default value
* @return		item value; dvalue if not found
*/
long
configparser_getsint(struct configparser *self, char *section_name, char *option, long dvalue) {
	struct configparser_item	*item;
	char				*fmt;
	long				value;

	if ((item = __configparser_get_item(self, section_name, option)) == NULL) {
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
* results must be freed (see configparser_sarray0_free()).
*
* @param self		configparser object
* @return		NULL-terminated array of strings; NULL on failure
*/
char **
configparser_options(struct configparser *self, char *section_name) {
	struct configparser_section	*section;
	char				**sarray0;
	int				i;

	if (((section = __configparser_find_section(self, section_name)) == NULL) 
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
	configparser_sarray0_free(sarray0);
	return NULL;
}

/**
* Read settings from named file. Comments, empty lines, ordering is
* not saved.
*
* @param filename		file name
* @return			configparser object; NULL on failure
*/
struct configparser *
configparser_read(char *filename) {
	struct configparser		*cp;
	struct configparser_section	*section;
	FILE				*fp;
	char				*section_name;
	char				buf[4096], *p0, *p1;

	if (((fp = fopen(filename, "r")) == NULL)
		|| ((cp = configparser_new()) == NULL)) {
		return NULL;
	}
	if ((configparser_add_section(cp, "DEFAULT")) < 0) {
		goto free_all;
	}
	section = __configparser_find_section(cp, "DEFAULT");

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
			if ((section = __configparser_find_section(cp, p0)) == NULL) {
				 if (configparser_add_section(cp, p0) < 0) {
				 	goto free_all;
				}
				section = __configparser_find_section(cp, p0);
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
			if (__configparser_section_set(section, p0, p1) == NULL) {
				goto free_all;
			}
		}
	}
	fclose(fp);
	return cp;
free_all:
	fclose(fp);
	configparser_free(cp);
	return NULL;
}

/**
* Free NULL-terminated string array.
*
* @param sarray0	NULL-terminated string array
*/
void
configparser_sarray0_free(char **sarray0) {
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
* must be freed (see configparser_sarray0_free()).
*
* @param self		configparser object
* @return		NULL-terminated array of strings; NULL on failure
*/
char **
configparser_sections(struct configparser *self) {
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
	configparser_sarray0_free(sarray0);
	return NULL;
}

/**
* Set option (name and value) for existing section.
*
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @param value		option value
* @return		0 on success; -1 on failure
*/
int
configparser_set(struct configparser *self, char *section_name, char *option, char *value) {
	struct configparser_section	*section;

	if (((section = __configparser_find_section(self, section_name)) == NULL)
		|| (__configparser_section_set(section, option, value) == NULL)) {
		return -1;
	}
	return 0;
}

/**
* Set option (name and value) for a new or existing section.
*
* @param self		configparser object
* @param section_name	section name
* @param option		option name
* @param value		option value
* @return		0 on success; -1 on failure
*/
int
configparser_set2(struct configparser *self, char *section_name, char *option, char *value) {
	struct configparser_section	*section;

	if ((!configparser_has_section(self, section_name))
		&& (configparser_add_section(self, section_name) < 0)) {
		return -1;
	}
	if (((section = __configparser_find_section(self, section_name)) == NULL)
		|| (__configparser_section_set(section, option, value) == NULL)) {
		return -1;
	}
	return 0;
}

/**
* Write configparser contents to file. Can be read in using
* configparser_read().
*
* @param self		configparser object
* @param fp		FILE * object
* @return		0 on success; -1 on failure
*/
int
configparser_write(struct configparser *self, FILE *fp) {
	struct configparser_section	**sections, *section;
	struct configparser_item	**items, *item;
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
