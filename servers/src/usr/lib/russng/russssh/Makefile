#
# servers/src/usr/lib/russng/russssh/Makefile
#

include ../../../../../../Makefile.inc

BINS=russssh_server
SERVERDIR=usr/lib/russng/russssh

all:	russssh_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russssh_server:	russssh_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
