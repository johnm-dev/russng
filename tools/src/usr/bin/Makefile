#
# Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

LIBS=
BINS=rudial ruls

ifndef RUSS_INCLUDE_DIR
        $(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
        $(error "error: missing RUSS_LIB_DIR")
endif

.PHONY: install

all: $(BINS)

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/usr/bin
	(cp $(BINS) $(INSTALL_DIR)/usr/bin)
	(cd $(INSTALL_DIR)/usr/bin; ln -fs rudial ruexec; ln -fs rudial ruhelp; ln -fs rudial ruinfo)
	(cd $(INSTALL_DIR)/usr/bin; ln -fs rudial ruexec; ln -fs rudial ruhelp; ln -fs rudial ruinfo)

rudial:	rudial.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rudial2:	rudial2.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

ruls:	ruls.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)
