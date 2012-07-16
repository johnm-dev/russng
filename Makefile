#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	doc install

all:
	(cd russlib; $(MAKE))
	(cd russservers; $(MAKE))
	(cd russtools; $(MAKE))
	(cd pyruss; $(MAKE))
	(cd rbruss; $(MAKE))

clean:
	(cd russlib; $(MAKE) clean)
	(cd russservers; $(MAKE) clean)
	(cd russtools; $(MAKE) clean)
	(cd pyruss; $(MAKE) clean)
	(cd rbruss; $(MAKE) clean)

doc:
	(cd russlib; $(MAKE) doc)
	#(cd russtools; $(MAKE) doc)
	#(cd pyruss; $(MAKE) doc)
	#(cd rbruss; $(MAKE) doc)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	(mkdir -p ${INSTALL_DIR})
	:
	(cd russlib; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd russservers; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd russtools; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd pyruss; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd rbruss; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
