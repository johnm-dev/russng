#
# servers/src/usr/lib/russng/russssh2/Makefile
#

include ../../../../../../Makefile.inc

BINS=russssh2_server
SERVERDIR=usr/lib/russng/russssh2

all:	russssh2_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russssh2_server:	russssh2_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
