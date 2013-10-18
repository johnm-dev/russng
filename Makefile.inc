#
# Makefile.inc
#

UNAME_SYSTEM=$(shell uname -s)

CFLAGS_RELAY=-DUSE_RUSS_FWD

ifeq ($(UNAME_SYSTEM),AIX)
	# OBJECT_MODE should be set externally
	CC=xlc_r
	CFLAGS=-DAIX -g $(CFLAGS_RELAY)
	CFLAGS_STATIC=$(CFLAGS) -bstatic
	LDFLAGS_STATIC=-bstatic
	LIBS_PLAT=-lbsd
	PYTHON=python2.6
	MAKE=gmake
endif

ifeq ($(UNAME_SYSTEM),Linux)
	#CC=arm-linux-gnueabi-gcc
	CC=gcc
	CFLAGS_RELAY=-DUSE_RUSS_RELAY
	CFLAGS=-DLINUX -g -DUSE_PAM $(CFLAGS_RELAY)
	CFLAGS_STATIC=$(CFLAGS) -static
	LDFLAGS_STATIC=-Wl,-Bstatic
	LDFLAGS_DYNAMIC=-Wl,-Bdynamic
	LIBS_PLAT=-lrt -lpam -lpam_misc
	MAKE=make
endif

ifeq ($(UNAME_SYSTEM),FreeBSD)
	CC=gcc
	CFLAGS=-DFREEBSD -g $(CFLAGS_RELAY)
	CFLAGS_STATIC=$(CFLAGS) -static
	LDFLAGS_STATIC=-Wl,-Bstatic
	LDFLAGS_DYNAMIC=-Wl,-Bdynamic
	MAKE=gmake
endif
