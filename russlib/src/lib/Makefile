#
# src/lib/Makefile
#
# expect INSTALL_DIR

include ../../../Makefile.inc

SRCS=addr.c conn.c encdec.c fd.c forwarder.c helpers.c io.c listener.c misc.c request.c socket.c
OBJS=addr.o conn.o encdec.o fd.o forwarder.o helpers.o io.o listener.o misc.o request.o socket.o

all: libruss.a libruss.so

clean:
	rm -f *.o *.a *.so

install:
	mkdir -p $(INSTALL_DIR)/lib
	(cp libruss.a $(INSTALL_DIR)/lib)
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		(cp libruss.so $(INSTALL_DIR)/lib); \
	fi

libruss.a: $(SRCS)
	$(CC) $(CFLAGS) -fPIC -c -I../include $(SRCS)
	$(AR) rv libruss.a $(OBJS)

libruss.so:	libruss.a
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		$(CC) -shared -o $@ $(OBJS); \
	fi

addr-test:	addr-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a
