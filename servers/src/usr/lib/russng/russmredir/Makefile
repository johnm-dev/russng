#
# servers/src/usr/lib/russng/russmredir/Makefile
#

include ../../../../../../Makefile.inc

BINS=russmredir_server
SERVERDIR=usr/lib/russng/russmredir

all:	russmredir_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russmredir_server:	russmredir_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
