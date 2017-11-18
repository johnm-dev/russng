#
# servers/src/usr/lib/russng/russexec/Makefile
#

include ../../../../../../Makefile.inc

BINS=russexec_server
SERVERDIR=usr/lib/russng/russexec

all:	russexec_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russexec_server:	russexec_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
