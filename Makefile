#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	docs install

all:
	(cd russlib; $(MAKE))
	(cd russtools; $(MAKE))
	(cd pyruss; $(MAKE))

clean:
	(cd russlib; $(MAKE) clean)
	(cd russtools; $(MAKE) clean)
	(cd pyruss; $(MAKE) clean)

docs:
	(cd russlib; $(MAKE) docs)
	#(cd russtools; $(MAKE) docs)
	#(cd pyruss; $(MAKE) docs)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	(mkdir -p ${INSTALL_DIR})
	:
	(cd russlib; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd russtools; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd pyruss; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
