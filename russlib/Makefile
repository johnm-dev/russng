#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	docs install

all:
	(cd src/lib; $(MAKE))

clean:
	(rm -rf docs)
	(cd src/lib; $(MAKE) clean)

docs:
	(doxygen doxygen.conf)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	:
	(mkdir -p $(INSTALL_DIR))
	(cd src/lib; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd src/include; mkdir -p ${INSTALL_DIR}/include; cp russ.h ${INSTALL_DIR}/include)
	(cd static; tar cf - share) | (cd $(INSTALL_DIR); tar xvf -)
	(mkdir -p $(INSTALL_DIR)/share/docs/russlib)
	(cd docs; tar cf - .) | (cd $(INSTALL_DIR)/share/docs/russlib; tar xvf -)
