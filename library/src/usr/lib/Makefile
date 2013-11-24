#
# src/lib/Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

SRCS=buffer.c conf.c connection.c encdec.c fd.c forwarder.c handlers.c \
	helpers.c io.c listener.c misc.c optable.c request.c socket.c spath.c \
	time.c server.c servicenode.c relay.c experimental.c relay2.c
OBJS=buffer.o conf.o connection.o encdec.o fd.o forwarder.o handlers.o \
	helpers.o io.o listener.o misc.o optable.o request.o socket.o spath.o \
	time.o server.o servicenode.o relay.o experimental.o relay2.o
LIBS=$(LIBS_PLAT)

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
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a $(LIBS)

time-test:	time-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a $(LIBS)
