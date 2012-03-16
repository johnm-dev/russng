#
# Makefile
#
# expect INSTALL_DIR

include ../../Makefile.inc

LIBS=
BINS=rusrv_barrier rusrv_echo

ifndef RUSS_INCLUDE_DIR
        $(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
        $(error "error: missing RUSS_LIB_DIR")
endif

.PHONY: install

all: rudial rusrv_barrier rusrv_echo

clean:
	rm -f rusrv_barrier rusrv_echo

install:
	(mkdir -p $(INSTALL_DIR)/bin; cp $(BINS) $(INSTALL_DIR)/bin)
	(cd $(INSTALL_DIR)/bin; ln -s rudial ruhelp; ln -s rudial ruinfo)

rudial:	rudial.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_echo:	rusrv_echo.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_barrier:	rusrv_barrier.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)
