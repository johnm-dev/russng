#
# servers/src/usr/lib/russng/russuredir/Makefile
#

include ../../../../../../Makefile.inc

BINS=russuredir_server
SERVERDIR=usr/lib/russng/russuredir

all:	russuredir_server

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russuredir_server:	russuredir_server.c
	$(CC) $(CFLAGS) -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)
