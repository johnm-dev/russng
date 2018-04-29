#
# servers/Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

ifndef RUSS_INCLUDE_DIR
        $(error "error: missing RUSS_INCLUDE_DIR")
endif

ifndef RUSS_LIB_DIR
        $(error "error: missing RUSS_LIB_DIR")
endif

SERVERS=russdebug russexec russpnet russproc russredir \
	russset russssh russssh2 russsuper russtee

all:
	for name in $(SERVERS); do \
		(cd src/usr/lib/russng/$${name}; $(MAKE) all) \
	done

clean:
	(rm -rf doc)
	for name in $(SERVERS); do \
		(cd src/usr/lib/russng/$${name}; $(MAKE) clean) \
	done

doc:
	#(doxygen doxygen.conf)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	:
	(mkdir -p "${INSTALL_DIR}")
	for name in $(SERVERS); do \
		(cd src/usr/lib/russng/$${name}; $(MAKE) install INSTALL_DIR=${INSTALL_DIR}) \
	done
	(cd static; tar cf - etc usr/bin usr/share) | (cd $(INSTALL_DIR); tar xvf -)
