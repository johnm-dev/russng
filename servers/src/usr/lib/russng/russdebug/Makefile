#
# servers/src/usr/lib/russng/russdebug/Makefile
#

include ../../../../../../Makefile.inc

BINS=russdebug_server
SERVERDIR=usr/lib/russng/russdebug

all:	russdebug_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russdebug_server:	russdebug_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
