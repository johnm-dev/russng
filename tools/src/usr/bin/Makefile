#
# Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

# rt for clock_gettime()
#LIBS=-lruss $(LIBS_PLAT)
LIBS_PTHREAD=-lruss-pthread -lruss -lpthread $(LIBS_PLAT)
BINS=rudial ruexec ruhelp ruinfo ruls rureap \
	ruspawn rustart rutuns rutunsr \
	russ_resolve_spath russ_str_resolve

ifndef RUSS_INCLUDE_DIR
	$(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
	$(error "error: missing RUSS_LIB_DIR")
endif

.PHONY:	all clean install

all:	$(BINS)

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/usr/bin
	(cp $(BINS) $(INSTALL_DIR)/usr/bin)

rudial:	rudial.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

ruexec: rudial
	cp rudial ruexec

ruhelp: rudial
	cp rudial ruhelp

ruinfo: rudial
	cp rudial ruinfo

ruls: rudial
	cp rudial ruls

#rudial2:	rudial2.c
#	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rureap:	rureap.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

ruspawn:	ruspawn.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rustart:	ruspawn
	cp ruspawn rustart

rutuns:	rutuns.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rutunsr:	rutuns
	cp rutuns rutunsr

russ_resolve_spath:	russ_resolve_spath.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russ_str_resolve:	russ_str_resolve.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
