/*
* russ_configparser.h
*/

#ifndef RUSS_CONFIGPARSER_H
#define RUSS_CONFIGPARSER_H

struct russ_configparser_item {
	char	*option;
	char	*value;
	union {
		long	ivalue;
		double	fvalue;
	};
};

struct russ_configparser_section {
	char				*name;
	struct russ_configparser_item	**items;
	int				len, cap;
};

struct russ_configparser {
	struct russ_configparser_section	**sections;
	int					len, cap;
};


struct russ_configparser *russ_configparser_new(void);
void russ_configparser_free(struct russ_configparser *);
int russ_configparser_add_section(struct russ_configparser *, char *);
int russ_configparser_has_section(struct russ_configparser *, char *);
char *russ_configparser_get(struct russ_configparser *, char *, char *, char *);
long russ_configparser_getint(struct russ_configparser *, char *, char *, long);
double russ_configparser_getfloat(struct russ_configparser *, char *, char *, double);
long russ_configparser_getsint(struct russ_configparser *, char *, char *, long);
char **russ_configparser_options(struct russ_configparser *, char *);
struct russ_configparser *russ_configparser_read(char *);
int russ_configparser_remove_option(struct russ_configparser *, char *, char *);
int russ_configparser_remove_section(struct russ_configparser *, char *);
int russ_configparser_set(struct russ_configparser *, char *, char *, char *);
int russ_configparser_set2(struct russ_configparser *, char *, char *, char *);
void russ_configparser_sarray0_free(char **);
char **russ_configparser_sections(struct russ_configparser *);
int russ_configparser_write(struct russ_configparser *, FILE *);

#endif /* RUSS_CONFIGPARSER_H */
