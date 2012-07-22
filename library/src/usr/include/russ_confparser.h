/*
* russ_confparser.h
*/

#ifndef RUSS_CONFIGPARSER_H
#define RUSS_CONFIGPARSER_H

struct russ_confparser_item {
	char	*option;
	char	*value;
	union {
		long	ivalue;
		double	fvalue;
	};
};

struct russ_confparser_section {
	char				*name;
	struct russ_confparser_item	**items;
	int				len, cap;
};

struct russ_confparser {
	struct russ_confparser_section	**sections;
	int					len, cap;
};


struct russ_confparser *russ_confparser_new(void);
void russ_confparser_free(struct russ_confparser *);
int russ_confparser_add_section(struct russ_confparser *, char *);
int russ_confparser_has_section(struct russ_confparser *, char *);
char *russ_confparser_get(struct russ_confparser *, char *, char *, char *);
long russ_confparser_getint(struct russ_confparser *, char *, char *, long);
double russ_confparser_getfloat(struct russ_confparser *, char *, char *, double);
long russ_confparser_getsint(struct russ_confparser *, char *, char *, long);
char **russ_confparser_options(struct russ_confparser *, char *);
int russ_confparser_read(struct russ_confparser *, char *);
int russ_confparser_remove_option(struct russ_confparser *, char *, char *);
int russ_confparser_remove_section(struct russ_confparser *, char *);
int russ_confparser_set(struct russ_confparser *, char *, char *, char *);
int russ_confparser_set2(struct russ_confparser *, char *, char *, char *);
void russ_confparser_sarray0_free(char **);
char **russ_confparser_sections(struct russ_confparser *);
int russ_confparser_write(struct russ_confparser *, FILE *);

#endif /* RUSS_CONFIGPARSER_H */
