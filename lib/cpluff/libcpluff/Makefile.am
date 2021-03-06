## Process this file with automake to produce Makefile.in.

# Copyright 2007 Johannes Lehtinen
# This Makefile is free software; Johannes Lehtinen gives unlimited
# permission to copy, distribute and modify it.

SUBDIRS = docsrc

LIBS = @LIBS_LIBCPLUFF@ @LTLIBINTL@ @LIBS@

CPPFLAGS = @CPPFLAGS@
CPPFLAGS += -I. -DCP_C_API=CP_EXPORT -DCP_HOST="\"$(host)\"" -DCP_DATADIR="\"$(datadir)\""
# the following symbols are clashing with mariadb symbols
CPPFLAGS += -Dhash_delete=kazlib_hash_delete -Dhash_free=kazlib_hash_free -Dhash_insert=kazlib_hash_insert -Dlist_delete=kazlib_list_delete

DOXYGEN = doxygen
DOXYGEN_SOURCE = cpluffdef.h $(srcdir)/cpluff.h $(srcdir)/docsrc/*.dox
DOXYGEN_STYLE = $(top_srcdir)/docsrc/doxygen.footer $(top_srcdir)/docsrc/doxygen.css

lib_LTLIBRARIES = libcpluff.la
libcpluff_la_SOURCES = psymbol.c pscan.c ploader.c pinfo.c pcontrol.c serial.c logging.c context.c cpluff.c util.c ../kazlib/list.c ../kazlib/list.h ../kazlib/hash.c ../kazlib/hash.h internal.h thread.h util.h defines.h
if POSIX_THREADS
libcpluff_la_SOURCES += thread_posix.c
endif
if WINDOWS_THREADS
libcpluff_la_SOURCES += thread_windows.c
endif
libcpluff_la_LDFLAGS = -no-undefined -version-info $(CP_C_LIB_VERSION)

include_HEADERS = cpluff.h cpluffdef.h

doc: refdoc

refdoc: doc/reference/c-api/index.html

doc/reference/c-api/index.html: $(DOXYGEN_SOURCE) $(top_srcdir)/doc/img/architecture.png docsrc/Doxyfile-ref $(DOXYGEN_STYLE)
	rm -rf doxygen-ref
	mkdir -p doxygen-ref
	cp -p $^ doxygen-ref
	cd doxygen-ref && $(DOXYGEN) Doxyfile-ref
	mkdir -p doc/reference
	rm -rf doc/reference/c-api
	mv doxygen-ref/html doc/reference/c-api
	rm -rf doxygen-ref

impldoc: doc/implementation/c-api/index.html

doc/implementation/c-api/index.html: $(srcdir)/*.h $(srcdir)/*.c cpluffdef.h ../config.h docsrc/Doxyfile-impl $(DOXYGEN_STYLE)
	rm -rf doxygen-impl
	mkdir -p doxygen-impl
	cp -p $^ doxygen-impl
	cd doxygen-impl && $(DOXYGEN) Doxyfile-impl
	mkdir -p doc/implementation
	rm -rf doc/implementation/c-api
	mv doxygen-impl/html doc/implementation/c-api
	rm -rf doxygen-impl

dist-hook: refdoc
	mkdir -p $(top_distdir)/doc/reference
	cp -rp doc/reference/c-api $(top_distdir)/doc/reference

clean-local:
	rm -rf doc

check-local:
	rc=0; \
		find '$(srcdir)' -name '*.c' -print | while read f; do \
			if grep -q -E 'cpi_(debug|warn|error)f?\(.*[^N]_\(' "$$f"; then \
				echo "invalid use of cpi_(debug|warn|error)f? macros in $$f."; \
				rc=1; \
			fi; \
		done; \
		exit $$rc

.PHONY: doc refdoc impldoc
