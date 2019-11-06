#
# Makefile.inc
#

UNAME_SYSTEM=$(shell uname -s)

CFLAGS_RELAY=-DUSE_RUSS_RELAY

ifeq ($(UNAME_SYSTEM),AIX)
	# OBJECT_MODE should be set externally
	CC=xlc_r
	CFLAGS=-D__RUSS_AIX__ -g $(CFLAGS_RELAY)
	CFLAGS_STATIC=$(CFLAGS) -bstatic
	CXX=xlc++_r
	CXXFLAGS=$(CFLAGS)
	CXXFLAGS_STATIC=$(CFLAGS_STATIC)
	LDFLAGS_STATIC=-bstatic
	LIBS_PLAT=-lbsd
	LIBS=$(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) $(LIBS_PLAT)
	LIBS_PTHREAD=$(RUSS_LIB_DIR)/libruss-pthread.a $(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) -lpthread $(LIBS_PLAT)

	PYTHON=python2.6
	MAKE=gmake
endif

ifeq ($(UNAME_SYSTEM),Darwin)
	CC=gcc
	CFLAGS=-D__RUSS_FREEBSD__ -g $(CFLAGS_RELAY)
	CFLAGS_STATIC=$(CFLAGS) -static
	CXX=g++
	CXXFLAGS=$(CFLAGS)
	CXXFLAGS_STATIC=$(CFLAGS_STATIC)
	LDFLAGS_STATIC=-static
	LDFLAGS_DYNAMIC=-dynamic
	LIBS=$(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) $(LIBS_PLAT)
	LIBS_PTHREAD=$(RUSS_LIB_DIR)/libruss-pthread.a $(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) -lpthread $(LIBS_PLAT)

	MAKE=gmake
endif

ifeq ($(UNAME_SYSTEM),FreeBSD)
	CC=gcc
	CFLAGS=-D__RUSS_FREEBSD__ -g $(CFLAGS_RELAY)
	CFLAGS_STATIC=$(CFLAGS) -static
	CXX=g++
	CXXFLAGS=$(CFLAGS)
	CXXFLAGS_STATIC=$(CFLAGS_STATIC)
	LDFLAGS_STATIC=-Wl,-Bstatic
	LDFLAGS_DYNAMIC=-Wl,-Bdynamic
	LIBS=$(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) $(LIBS_PLAT)
	LIBS_PTHREAD=$(RUSS_LIB_DIR)/libruss-pthread.a $(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) -lpthread $(LIBS_PLAT)

	MAKE=gmake
endif

ifeq ($(UNAME_SYSTEM),Linux)
	#CC=arm-linux-gnueabi-gcc
	CC=gcc
	CFLAGS=-D__RUSS_LINUX__ -g -DUSE_PAM $(CFLAGS_RELAY)
	CFLAGS_STATIC=$(CFLAGS) -static
	CXX=g++
	CXXFLAGS=$(CFLAGS)
	CXXFLAGS_STATIC=$(CFLAGS_STATIC)
	LDFLAGS_STATIC=-Wl,-Bstatic
	LDFLAGS_DYNAMIC=-Wl,-Bdynamic
	LIBS_PLAT=-lrt -lpam -lpam_misc
	LIBS=$(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) $(LIBS_PLAT)
	LIBS_PTHREAD=$(RUSS_LIB_DIR)/libruss-pthread.a $(RUSS_LIB_DIR)/libruss.a $(LDFLAGS_DYNAMIC) -lpthread $(LIBS_PLAT)

	MAKE=make
endif
