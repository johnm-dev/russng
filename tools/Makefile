#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

STATIC_FILES = usr/bin \
	etc \
	usr/lib \
	usr/sbin \
	usr/share

.PHONY:	all clean doc install

all:
	(cd src/usr/bin; $(MAKE) all)

clean:
	(rm -rf doc)
	(cd src/usr/bin; $(MAKE) clean)

doc:
	#(doxygen doxygen.conf)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	:
	(mkdir -p "${INSTALL_DIR}")
	(cd src/usr/bin; $(MAKE) install INSTALL_DIR=${INSTALL_DIR})
	(cd static; tar cf - ${STATIC_FILES}) | (cd $(INSTALL_DIR); tar xvf -)
