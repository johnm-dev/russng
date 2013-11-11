#
# Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

# rt for clock_gettime()
LIBS=-lruss -lpthread $(LIBS_PLAT)
BINS=dial_ev dial_inouterr

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
	(cd $(INSTALL_DIR)/usr/bin; ln -fs rudial ruexec; ln -fs rudial ruhelp; ln -fs rudial ruinfo)
	(cd $(INSTALL_DIR)/usr/bin; ln -fs rudial ruexec; ln -fs rudial ruhelp; ln -fs rudial ruinfo)

dial_ev: dial_ev.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

dial_inouterr: dial_inouterr.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
