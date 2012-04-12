#
# Makefile.inc
#

UNAME_SYSTEM=$(shell uname -s)

ifeq ($(UNAME_SYSTEM),AIX)
	# OBJECT_MODE should be set externally
	CC=xlc_r
	#CFLAGS=-DAIX -g -bstatic
	CFLAGS=-DAIX -g
	LIBS=-lbsd
	PYTHON=python2.6
	MAKE=gmake
endif

ifeq ($(UNAME_SYSTEM),Linux)
	CC=gcc
	CFLAGS=-DLINUX -g -static
	MAKE=make
endif

ifeq ($(UNAME_SYSTEM),FreeBSD)
	CC=gcc
	CFLAGS=-DFREEBSD -g -static
	MAKE=gmake
endif
