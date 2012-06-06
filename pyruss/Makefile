#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	docs install

all:
	:

clean:
	:

docs:
	#(doxygen doxygen.conf)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	:
	(mkdir -p $(INSTALL_DIR))
	(cd static; tar cf - bin lib) | (cd $(INSTALL_DIR); tar xvf -)
