#
# Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

LIBS=
BINS=rusrv_barrier rusrv_debug rusrv_echo rusrv_exec rusrv_pexec rusrv_relay rusrv_relay_backend rusrv_srelay rusrv_srelay_backend

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

configparser.o:	configparser.c
	$(CC) $(CFLAGS) -fPIC -c -I. $<

disp.o:	disp.c
	$(CC) $(CFLAGS) -fPIC -c -I. -I$(RUSS_INCLUDE_DIR) $<

rusrv_barrier:	rusrv_barrier.c configparser.o
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_debug:	rusrv_debug.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_echo:	rusrv_echo.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_exec:	rusrv_exec.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_pexec:	rusrv_pexec.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_relay:	rusrv_relay.c configparser.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) -lruss -lpthread -lssh

rusrv_relay_backend: rusrv_relay_backend.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) -lruss -lpthread

rusrv_srelay:	rusrv_srelay.c configparser.o disp.o
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) -lruss -lpthread

rusrv_srelay_backend: rusrv_srelay_backend.c disp.o
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) -lruss -lpthread

rusrv_ssh:	rusrv_ssh.c configparser.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) -lruss -lpthread
