#
# servers/src/usr/lib/russng/russalive/Makefile
#

include ../../../../../../Makefile.inc

BINS=russalive_server-fork russalive_server-thread
SERVERDIR=usr/lib/russng/russalive

all:	russalive_server-fork russalive_server-thread

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/$(SERVERDIR)
	(cp $(BINS) $(INSTALL_DIR)/$(SERVERDIR))

russalive_server-fork:	russalive_server.c
	$(CC) $(CFLAGS) -DRUSSALIVE_FORK=1 -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS)

russalive_server-thread:	russalive_server.c
	$(CC) $(CFLAGS) -DRUSSALIVE_THREAD=1 -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS_PTHREAD)
