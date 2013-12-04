#
# servers/futils/src/usr/bin/Makefile
#
# expect INSTALL_DIR

include ../../../../../Makefile.inc

BINS=rusrv_futils

ifndef RUSS_INCLUDE_DIR
        $(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
        $(error "error: missing RUSS_LIB_DIR")
endif

.PHONY: install

all: $(BINS)

clean:
	rm -f $(BINS) *.o

install:
	mkdir -p $(INSTALL_DIR)/usr/bin
	(cp $(BINS) $(INSTALL_DIR)/usr/bin)

rusrv_futils:	rusrv_futils.c
	$(CC) $(CFLAGS) -DOPENSSL -o $@ -I$(RUSS_INCLUDE_DIR) $< -L$(RUSS_LIB_DIR) $(LDFLAGS) $(LIBS) -lssl -lcrypto