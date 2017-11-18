#
# servers/src/usr/lib/russng/russpass/Makefile
#

include ../../../../../../Makefile.inc

BINS=russpass_server
SERVERDIR=usr/lib/russng/russpass

all:	russpass_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russpass_server:	russpass_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
