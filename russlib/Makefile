#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	docs install

all:	docs
	(cd src/usr/lib; $(MAKE))

clean:
	(rm -rf docs)
	(cd src/usr/lib; $(MAKE) clean)

docs:
	@if [ `doxygen -h 2> /dev/null` ]; then \
		(doxygen doxygen.conf); \
	else \
		echo "warning: doxygen not available to produce documentation"; \
	fi

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	:
	(mkdir -p $(INSTALL_DIR))
	(cd src/usr/lib; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd src/usr/include; mkdir -p ${INSTALL_DIR}/include; cp russ.h russ_priv.h ${INSTALL_DIR}/include)
	(cd static; tar cf - etc usr/share) | (cd $(INSTALL_DIR); tar xvf -)
	(mkdir -p $(INSTALL_DIR)/usr/share/docs/russlib)
	if [ -d docs ]; then \
		(cd docs; tar cf - .) | (cd $(INSTALL_DIR)/usr/share/docs/russlib; tar xvf -) \
	fi
