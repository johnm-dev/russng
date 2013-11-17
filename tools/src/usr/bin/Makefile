#
# Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

# rt for clock_gettime()
LIBS=-lruss -lpthread $(LIBS_PLAT)
BINS=rudial ruexec ruhelp ruinfo ruls russ_resolve_spath

ifndef RUSS_INCLUDE_DIR
	$(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
	$(error "error: missing RUSS_LIB_DIR")
endif

.PHONY:	install

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

russ_resolve_spath:	russ_resolve_spath.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)