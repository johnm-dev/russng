#
# Makefile
#
# expect INSTALL_DIR

include ../../../Makefile.inc

LIBS=
BINS=rudial ruls rusrv_barrier rusrv_debug rusrv_echo rusrv_exec rusrv_pexec rusrv_relay rusrv_relay_backend

ifndef RUSS_INCLUDE_DIR
        $(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
        $(error "error: missing RUSS_LIB_DIR")
endif

.PHONY: install

all: $(BINS)

clean:
	rm -f $(BINS)

install:
	mkdir -p $(INSTALL_DIR)/bin
	(cp $(BINS) $(INSTALL_DIR)/bin)
	(cd $(INSTALL_DIR)/bin; ln -fs rudial ruexec)
	(cd $(INSTALL_DIR)/bin; ln -fs ruls ruhelp; ln -fs ruls ruinfo)

rudial:	rudial.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rudial2:	rudial2.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

ruls:	ruls.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_barrier:	rusrv_barrier.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_debug:	rusrv_debug.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_echo:	rusrv_echo.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_exec:	rusrv_exec.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_pexec:	rusrv_pexec.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

configparser.o:	configparser.c
	$(CC) $(CFLAGS) -fPIC -c $<

rusrv_relay:	rusrv_relay.c configparser.o
	$(CC) $(XCFLAGS) -Bdynamic -o $@ $^ -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) $(LDFLAGS) -lruss -lpthread -lssh

rusrv_relay_backend: rusrv_relay_backend.c
	$(CC) $(CFLAGS) -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) $(LDFLAGS) -lruss -lpthread
