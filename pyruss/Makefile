#
# Makefile
#

# this can only be done in the first Makefile
HERE_FILE:="$(CURDIR)/$(strip $(MAKEFILE_LIST))"
HERE_DIR:=$(shell dirname $(HERE_FILE))

.PHONY:	doc install

all:
	:

clean:
	(rm -rf doc)

doc:
	# with epydoc
	mkdir -p doc/epydoc
	(cd doc/epydoc; epydoc $(HERE_DIR)/static/usr/lib/python/pyruss)

	# with pydoc
	#mkdir -p doc/pydoc
	#(cd doc/pydoc; PYTHONPATH=$(HERE_DIR)/static/usr/lib/python pydoc -w pyruss pyruss.base pyruss.bindings pyruss.conf pyruss.server)
	# strip local filename references
	#(cd doc/pydoc; for n in *.html; do sed -i 's/<a href=\"file.*<\/a>//g' $$n; done)

install:
	if test "${INSTALL_DIR}" = ""; then echo "error: INSTALL_DIR not defined"; exit 1; fi
	:
	(mkdir -p $(INSTALL_DIR))
	(cd static; tar cf - usr/bin usr/lib) | (cd $(INSTALL_DIR); tar xvf -)
	(mkdir -p $(INSTALL_DIR)/usr/share/doc/pyruss)
	if [ -d doc ]; then \
		(cd doc; tar cf - .) | (cd $(INSTALL_DIR)/usr/share/doc/pyruss; tar xvf -) \
	fi
