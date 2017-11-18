#
# servers/src/usr/lib/russng/russpnet/Makefile
#

include ../../../../../../Makefile.inc

BINS=russpnet_server
SERVERDIR=usr/lib/russng/russpnet

all:	russpnet_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russpnet_server:	russpnet_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
