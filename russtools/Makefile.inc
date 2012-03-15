#
# Makefile.inc
#

UNAME_SYSTEM=$(shell uname -s)

ifeq ($(UNAME_SYSTEM),AIX)
	CC=xlc_r -q32
	CFLAGS="-DAIX"
	LIBS="-lbsd"
	OBJECT_MODE=32
	PYTHON=python2.6
	MAKE=gmake
endif

ifeq ($(UNAME_SYSTEM),Linux)
	CC=gcc
	CFLAGS="-DLINUX"
endif
