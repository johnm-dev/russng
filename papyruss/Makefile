#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

ifndef INSTALL_DIR
	INSTALL_DIR=$(HERE_DIR)/install
endif

.PHONY:	install

all:
	:

clean:
	:

docs:
	#(doxygen doxygen.conf)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	if test -e ${INSTALL_DIR}; then rm -rf ${INSTALL_DIR}; fi
	:
	(mkdir -p $(INSTALL_DIR))
	(cd static; tar cf - lib) | (cd $(INSTALL_DIR); tar xvf -)
