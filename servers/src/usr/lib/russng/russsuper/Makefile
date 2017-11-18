#
# servers/src/usr/lib/russng/russsuper/Makefile
#

include ../../../../../../Makefile.inc

BINS=russsuper_server
SERVERDIR=usr/lib/russng/russsuper

all:	russsuper_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russsuper_server:	russsuper_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
