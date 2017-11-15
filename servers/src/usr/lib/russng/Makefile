#
# servers/src/usr/lib/russng/Makefile
#
# expect INSTALL_DIR

include ../../../../../Makefile.inc

BINS=russdebug_server russexec_server russproc_server \
	russpnet_server russredir_server russset_server \
	russssh_server russssh2_server russsuper_server \
	russtee_server

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
	mkdir -p $(INSTALL_DIR)/usr/lib/russng
	(cp $(BINS) $(INSTALL_DIR)/usr/lib/russng)

russdebug_server:	russdebug_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russexec_server:	russexec_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russpass_server:	russpass_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russproc_server:	russproc_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russpnet_server:	russpnet_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russredir_server:	russredir_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russset_server:	russset_server.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russssh_server:	russssh_server.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russssh2_server:	russssh2_server.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russsuper_server:	russsuper_server.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russtee_server:	russtee_server.c
	$(CC) $(CFLAGS) -o $@ -I. -I$(RUSS_INCLUDE_DIR) $^ -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
