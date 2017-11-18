#
# servers/src/usr/lib/russng/russtee/Makefile
#

include ../../../../../../Makefile.inc

BINS=russtee_server
SERVERDIR=usr/lib/russng/russtee

all:	russtee_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russtee_server:	russtee_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
