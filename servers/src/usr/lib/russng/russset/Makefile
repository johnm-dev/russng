#
# servers/src/usr/lib/russng/russset/Makefile
#

include ../../../../../../Makefile.inc

BINS=russset_server
SERVERDIR=usr/lib/russng/russset

all:	russset_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russset_server:	russset_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
