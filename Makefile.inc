#
# Makefile.inc
#

UNAME_SYSTEM=$(shell uname -s)

ifeq ($(UNAME_SYSTEM),AIX)
	# OBJECT_MODE should be set externally
	CC=xlc_r
	CFLAGS=-DAIX -g
	CFLAGS_STATIC=$(CFLAGS) -bstatic
	LIBS_PLAT=-lbsd
	PYTHON=python2.6
	MAKE=gmake
endif

ifeq ($(UNAME_SYSTEM),Linux)
	CC=gcc
	CFLAGS=-DLINUX -g
	CFLAGS_STATIC=$(CFLAGS) -static
	LIBS_PLAT=-lrt
	MAKE=make
endif

ifeq ($(UNAME_SYSTEM),FreeBSD)
	CC=gcc
	CFLAGS=-DFREEBSD -g
	CFLAGS_STATIC=$(CFLAGS) -static
	MAKE=gmake
endif
