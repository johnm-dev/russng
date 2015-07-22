#
# servers/base/src/usr/bin/Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

BINS=rusrv_debug rusrv_exec rusrv_proc rusrv_pnet \
	rusrv_redir rusrv_set rusrv_ssh \
	rusrv_super rusrv_tee

ifndef RUSS_INCLUDE_DIR
        $(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
        $(error "error: missing RUSS_LIB_DIR")
endif

.PHONY: all clean install

all: $(BINS)

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/usr/bin
	(cp $(BINS) $(INSTALL_DIR)/usr/bin)

rusrv_debug:	rusrv_debug.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_exec:	rusrv_exec.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_pass:	rusrv_pass.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_proc:	rusrv_proc.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_pnet:	rusrv_pnet.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_redir:	rusrv_redir.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_set:	rusrv_set.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_ssh:	rusrv_ssh.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_super:	rusrv_super.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

rusrv_tee:	rusrv_tee.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
