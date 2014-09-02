#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	doc install

all:
	(cd base; $(MAKE))
	#(cd futils; $(MAKE))

clean:
	(cd base; $(MAKE) clean)
	#(cd futils; $(MAKE) clean)

doc:
	#(doxygen doxygen.conf)

install:
	(cd base; $(MAKE) install)
	#(cd futils; $(MAKE) install)
