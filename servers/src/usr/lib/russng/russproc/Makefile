#
# servers/src/usr/lib/russng/russproc/Makefile
#

include ../../../../../../Makefile.inc

BINS=russproc_server
SERVERDIR=usr/lib/russng/russproc

all:	russproc_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russproc_server:	russproc_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
