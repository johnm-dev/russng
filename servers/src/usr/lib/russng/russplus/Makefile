#
# servers/src/usr/lib/russng/russplus/Makefile
#

include ../../../../../../Makefile.inc

BINS=russplus_server
SERVERDIR=usr/lib/russng/russplus

all:	russplus_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russplus_server:	russplus_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
