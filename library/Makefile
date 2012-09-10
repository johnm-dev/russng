#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	doc install

all:
	(cd src/usr/lib; $(MAKE))

clean:
	(rm -rf doc)
	(cd src/usr/lib; $(MAKE) clean)

doc:
	if [ "`which doxygen`" = ""  ]; then \
		echo "warning: doxygen not available to produce documentation"; \
		exit 1; \
	else \
		(doxygen doxygen.conf); \
	fi

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	:
	(mkdir -p $(INSTALL_DIR))
	(cd src/usr/lib; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd src/usr; cp -r include ${INSTALL_DIR}/usr)
	(cd static; tar cf - etc usr/share) | (cd $(INSTALL_DIR); tar xvf -)
	(mkdir -p $(INSTALL_DIR)/usr/share/doc/russ)
	if [ -d doc ]; then \
		(cd doc; tar cf - .) | (cd $(INSTALL_DIR)/usr/share/doc/russ; tar xvf -) \
	fi
