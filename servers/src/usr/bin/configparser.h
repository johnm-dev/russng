/*
* configparser.h
*/

#ifndef CONFIGPARSER_H
#define CONFIGPARSER_H

struct configparser_item {
	char	*option;
	char	*value;
	union {
		long	ivalue;
		double	fvalue;
	};
};

struct configparser_section {
	char				*name;
	struct configparser_item	**items;
	int				len, cap;
};

struct configparser {
	struct configparser_section	**sections;
	int				len, cap;
};


struct configparser *configparser_new(void);
void configparser_free(struct configparser *);
int configparser_add_section(struct configparser *, char *);
int configparser_has_section(struct configparser *, char *);
char *configparser_get(struct configparser *, char *, char *, char *);
long configparser_getint(struct configparser *, char *, char *, long);
double configparser_getfloat(struct configparser *, char *, char *, double);
long configparser_getsint(struct configparser *, char *, char *, long);
char **configparser_options(struct configparser *, char *);
struct configparser *configparser_read(char *);
int configparser_remove_option(struct configparser *, char *, char *);
int configparser_remove_section(struct configparser *, char *);
int configparser_set(struct configparser *, char *, char *, char *);
int configparser_set2(struct configparser *, char *, char *, char *);
void configparser_sarray0_free(char **);
char **configparser_sections(struct configparser *);
int configparser_write(struct configparser *, FILE *);

#endif /* CONFIGPARSER_H */
