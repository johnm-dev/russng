/*
* russ_conf.h
*/

#ifndef RUSS_CONF_H
#define RUSS_CONF_H

struct russ_conf_item {
	char	*option;
	char	*value;
	union {
		long	ivalue;
		double	fvalue;
	};
};

struct russ_conf_section {
	char			*name;
	struct russ_conf_item	**items;
	int			len, cap;
};

struct russ_conf 
	struct russ_conf_section	**sections;
	int				len, cap;
};


struct russ_conf *russ_conf_new(void);
void russ_conf_free(struct russ_conf *);
int russ_conf_add_section(struct russ_conf *, char *);
int russ_conf_has_section(struct russ_conf *, char *);
char *russ_conf_get(struct russ_conf *, char *, char *, char *);
long russ_conf_getint(struct russ_conf *, char *, char *, long);
double russ_conf_getfloat(struct russ_conf *, char *, char *, double);
long russ_conf_getsint(struct russ_conf *, char *, char *, long);
char **russ_conf_options(struct russ_conf *, char *);
int russ_conf_read(struct russ_conf *, char *);
int russ_conf_remove_option(struct russ_conf *, char *, char *);
int russ_conf_remove_section(struct russ_conf *, char *);
int russ_conf_set(struct russ_conf *, char *, char *, char *);
int russ_conf_set2(struct russ_conf *, char *, char *, char *);
void russ_conf_sarray0_free(char **);
char **russ_conf_sections(struct russ_conf *);
int russ_conf_write(struct russ_conf *, FILE *);

#endif /* RUSS_CONF_H */
