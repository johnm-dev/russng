#
# src/lib/Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

SRCS=addr.c conf.c conn.c encdec.c fd.c forwarder.c helpers.c io.c listener.c misc.c request.c socket.c time.c
OBJS=addr.o conf.o conn.o encdec.o fd.o forwarder.o helpers.o io.o listener.o misc.o request.o socket.o time.o
LIBS=-lrt

all: libruss.a libruss.so

clean:
	rm -f *.o *.a *.so

install:
	mkdir -p $(INSTALL_DIR)/usr/lib
	(cp libruss.a $(INSTALL_DIR)/usr/lib)
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		(cp libruss.so $(INSTALL_DIR)/usr/lib); \
	fi

libruss.a: $(SRCS)
	$(CC) $(CFLAGS) -fPIC -c -I../include $(SRCS)
	$(AR) rv libruss.a $(OBJS)

libruss.so:	libruss.a
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		$(CC) -shared -o $@ $(OBJS) $(LIBS); \
	fi

addr-test:	addr-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a

time-test:	time-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a -lrt