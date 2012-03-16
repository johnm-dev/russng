#
# src/lib/Makefile
#
# expect INSTALL_DIR

include ../../Makefile.inc

SRCS=conn.c encdec.c io.c misc.c request.c socket.c server.c
OBJS=conn.o encdec.o io.o misc.o request.o socket.o server.o

all: libruss.a libruss.so

clean:
	rm -f *.o *.a *.so

install:
	mkdir -p $(INSTALL_DIR)/lib
	(cp libruss.a libruss.so $(INSTALL_DIR)/lib)

libruss.a: $(SRCS)
	$(CC) $(CFLAGS) -fPIC -g -c -I../include $(SRCS)
	$(AR) rv libruss.a $(OBJS)

libruss.so:	libruss.a
	$(CC) -shared -o $@ $(OBJS)
