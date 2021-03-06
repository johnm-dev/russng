#
# src/lib/Makefile
#
# expect INSTALL_DIR

include ../../../../Makefile.inc

SRCS=args.c buf.c cconn.c conf.c convenience.c debug.c \
	encdec.c env.c \
	fd.c io.c memory.c misc.c optable.c relay.c req.c \
	sarray0.c sconn.c sess.c socket.c spath.c start.c str.c \
	svcnode.c svr.c time.c user.c
	#experimental.c
SRCS_FORK=$(SRCS) svr-fork.c
SRCS_PTHREAD=$(SRCS) svr-pthread.c

OBJS=args.o buf.o cconn.o conf.o convenience.o debug.o \
	encdec.o env.o \
	fd.o io.o memory.o misc.o optable.o relay.o req.o \
	sarray0.o sconn.o sess.o socket.o spath.o start.o str.o \
	svcnode.o svr.o time.o user.o
	#experimental.o
OBJS_FORK=$(OBJS) svr-fork.o
OBJS_PTHREAD=$(OBJS) svr-pthread.o
LIBS=$(LIBS_PLAT)

.PHONY: all clean install

all: libruss.a libruss.so libruss-pthread.a libruss-pthread.so

clean:
	rm -f *.o *.a *.so

install:
	mkdir -p $(INSTALL_DIR)/usr/lib
	(cp libruss.a libruss-pthread.a $(INSTALL_DIR)/usr/lib)
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		(cp libruss.so libruss-pthread.so $(INSTALL_DIR)/usr/lib); \
	fi

libruss.a: $(SRCS_FORK)
	$(CC) $(CFLAGS) -fPIC -c -I../include $(SRCS_FORK)
	$(AR) rv $@ $(OBJS) $(OBJS_FORK)

libruss.so:	libruss.a
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		$(CC) -shared -o $@ $(OBJS_FORK) $(LIBS); \
	fi

libruss-pthread.a: $(SRCS_PTHREAD)
	$(CC) $(CFLAGS) -fPIC -c -I../include $(SRCS_PTHREAD)
	$(AR) rv $@ $(OBJS_PTHREAD)

libruss-pthread.so:	libruss-pthread.a
	@if [ "$(UNAME_SYSTEM)" != "AIX" ]; then \
		$(CC) -shared -o $@ $(OBJS_PTHREAD) $(LIBS); \
	fi

addr-test:	addr-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a $(LIBS)

time-test:	time-test.c libruss.a
	$(CC) $(CFLAGS) -o $@ -I../include $< libruss.a $(LIBS)
