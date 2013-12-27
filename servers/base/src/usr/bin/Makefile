#
# servers/base/src/usr/bin/Makefile
#
# expect INSTALL_DIR

include ../../../../../Makefile.inc

BINS=rusrv_debug rusrv_echo rusrv_exec rusrv_pnet rusrv_set rusrv_ssh rusrv_super

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

disp.o:	disp.c
	$(CC) $(CFLAGS) -fPIC -c -I. -I$(RUSS_INCLUDE_DIR) $<

rusrv_barrier:	rusrv_barrier.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_debug:	rusrv_debug.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_echo:	rusrv_echo.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_exec:	rusrv_exec.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_pass:	rusrv_pass.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_pexec:	rusrv_pexec.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_pnet:	rusrv_pnet.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_relay:	rusrv_relay.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS) -lssh

rusrv_relay_backend: rusrv_relay_backend.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_set:	rusrv_set.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_srelay:	rusrv_srelay.c disp.o
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_srelay_backend: rusrv_srelay_backend.c disp.o
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_ssh:	rusrv_ssh.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_super:	rusrv_super.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)