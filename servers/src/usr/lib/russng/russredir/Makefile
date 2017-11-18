#
# servers/src/usr/lib/russng/russredir/Makefile
#

include ../../../../../../Makefile.inc

BINS=russredir_server
SERVERDIR=usr/lib/russng/russredir

all:	russredir_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russredir_server:	russredir_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
