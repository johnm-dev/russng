#
# src/lib/Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

SRCS=buf.c cconn.c conf.c encdec.c fd.c handlers.c \
	helpers.c io.c misc.c optable.c relay.c req.c \
	sarray0.c sconn.c socket.c spath.c str.c svcnode.c svr.c \
	time.c
	#experimental.c
SRCS_PTHREAD=svr-pthread.c
OBJS=buf.o cconn.o conf.o encdec.o fd.o handlers.o \
	helpers.o io.o misc.o optable.o relay.o req.o \
	sarray0.o sconn.o socket.o spath.o str.o svcnode.o svr.o \
	time.o
	#experimental.o
OBJS_PTHREAD=svr-pthread.o
LIBS=$(LIBS_PLAT)

all: libruss.a libruss.so libruss-pthread.a libruss-pthread.so

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
	$(AR) rv $@ $(OBJS)

libruss.so:	libruss.a
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		$(CC) -shared -o $@ $(OBJS) $(LIBS); \
	fi

libruss-pthread.a: $(SRCS_PTHREAD)
	$(CC) $(CFLAGS) -fPIC -c -I../include $(SRCS_PTHREAD)
	$(AR) rv $@ $(OBJS)

libruss-pthread.so:	libruss-pthread.a
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		$(CC) -shared -o $@ $(OBJS_PTHREAD) $(LIBS); \
	fi

addr-test:	addr-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a $(LIBS)

time-test:	time-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a $(LIBS)
