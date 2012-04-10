#
# Makefile
#
# expect INSTALL_DIR

include ../../Makefile.inc

LIBS=
BINS=rudial ruls rusrv_barrier rusrv_debug rusrv_echo rusrv_exec rusrv_pexec

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
	(cd $(INSTALL_DIR)/bin; ln -s rudial ruexec)
	(cd $(INSTALL_DIR)/bin; ln -s ruls ruhelp; ln -s ruls ruinfo)

rudial:	rudial.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rudial2:	rudial2.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

ruls:	ruls.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_barrier:	rusrv_barrier.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_debug:	rusrv_debug.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_echo:	rusrv_echo.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_exec:	rusrv_exec.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)

rusrv_pexec:	rusrv_pexec.c
	$(CC) -static $(CFLAGS) -g -o $@ $< -I$(RUSS_INCLUDE_DIR) -L$(RUSS_LIB_DIR) -lruss -lpthread $(LDFLAGS)
