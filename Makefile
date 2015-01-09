#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

export RUSS_INCLUDE_DIR:=$(HERE_DIR)/library/src/usr/include
export RUSS_LIB_DIR:=$(HERE_DIR)/library/src/usr/lib

.PHONY:	doc install

all:
	(cd library; $(MAKE))
	(cd library; $(MAKE) doc)
	(cd servers; $(MAKE))
	(cd tools; $(MAKE))
	(cd pyruss; $(MAKE))
	#(cd tests; $(MAKE))

clean:
	(cd library; $(MAKE) clean)
	(cd servers; $(MAKE) clean)
	(cd tools; $(MAKE) clean)
	(cd pyruss; $(MAKE) clean)
	#(cd tests; $(MAKE) clean)

doc:
	(cd library; $(MAKE) doc)
	#(cd servers; $(MAKE) doc)
	#(cd tools; $(MAKE) doc)
	#(cd pyruss; $(MAKE) doc)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	(mkdir -p ${INSTALL_DIR})
	:
	(cd library; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd servers; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd tools; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd pyruss; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
