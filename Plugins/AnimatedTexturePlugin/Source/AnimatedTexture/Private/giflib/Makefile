# Top-level Unix makefile for the GIFLIB package
# Should work for all Unix versions
#
# If your platform has the OpenBSD reallocarray(3) call, you may
# add -DHAVE_REALLOCARRAY to CFLAGS to use that, saving a bit
# of code space in the shared library.
#
# SPDX-FileCopyrightText: Copyright (C) Eric S. Raymond <esr@thyrsus.com>
# SPDX-License-Identifier: MIT

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
INCDIR = $(PREFIX)/include
LIBDIR = $(PREFIX)/lib
MANDIR = $(PREFIX)/share/man
DOCDIR = $(PREFIX)/share/doc/giflib

CC ?= gcc
OFLAGS = -g -fno-inline #-fsanitize=address
OFLAGS  = -O2
CFLAGS  += -std=gnu99 -fPIC -Wall $(OFLAGS)
#LDFLAGS += -fsanitize=address

SHELL = /bin/sh
TAR = tar
INSTALL = install

# No user-serviceable parts below this line

VERSION:=$(shell ./getversion)
LIBMAJOR=7
LIBMINOR=2
LIBPOINT=0
LIBVER=$(LIBMAJOR).$(LIBMINOR).$(LIBPOINT)

SOURCES = dgif_lib.c egif_lib.c gifalloc.c gif_err.c gif_font.c \
	gif_hash.c openbsd-reallocarray.c quantize.c
HEADERS = gif_hash.h  gif_lib.h  gif_lib_private.h
OBJECTS = $(SOURCES:.c=.o)

USOURCES = qprintf.c getarg.c
UHEADERS = getarg.h
UOBJECTS = $(USOURCES:.c=.o)

UNAME:=$(shell uname)

# Rules

.PHONY: all distcheck check reflow cppcheck spellcheck
.PHONY: install install-bin install-include ibnstall-lib install-man install-doc
.PHONY: uninstall uninstall-bin uninstall-include uninstall-lib uninstall-man uninstall-doc
.PHONY: version dist release refresh

# Build

# Some utilities are installed
INSTALLABLE = \
	gifbuild \
	giffix \
	giftext \
	giftool \
	gifclrmp

# Some utilities are only used internally for testing.
# There is a parallel list in doc/Makefile.
# These are all candidates for removal in future releases.
UTILS = $(INSTALLABLE) \
	gifbg \
	gifcolor \
	gifecho \
	giffilter \
	gifhisto \
	gifinto \
	gifsponge \
	gifwedge \
	gif2rgb

LDLIBS=libgif.a -lm

MANUAL_PAGES_1 = \
	doc/gifbuild.xml \
	doc/gifclrmp.xml \
	doc/giffix.xml \
	doc/giftext.xml \
	doc/giftool.xml

MANUAL_PAGES_7 = \
	doc/giflib.xml

MANUAL_PAGES = $(MANUAL_PAGES_1) $(MANUAL_PAGES_7)
MANUAL_PAGES_1_MAN = $(MANUAL_PAGES_1:%.xml=%.1)
MANUAL_PAGES_7_MAN = $(MANUAL_PAGES_7:%.xml=%.7)

SOEXTENSION	= so
LIBGIFSO	= libgif.$(SOEXTENSION)
LIBGIFSOMAJOR	= libgif.$(SOEXTENSION).$(LIBMAJOR)
LIBGIFSOVER	= libgif.$(SOEXTENSION).$(LIBVER)
LIBUTILSO	= libutil.$(SOEXTENSION)
LIBUTILSOMAJOR	= libutil.$(SOEXTENSION).$(LIBMAJOR)
ifeq ($(UNAME), Darwin)
SOEXTENSION	= dylib
LIBGIFSO        = libgif.$(SOEXTENSION)
LIBGIFSOMAJOR   = libgif.$(LIBMAJOR).$(SOEXTENSION)
LIBGIFSOVER	= libgif.$(LIBVER).$(SOEXTENSION)
LIBUTILSO	= libutil.$(SOEXTENSION)
LIBUTILSOMAJOR	= libutil.$(LIBMAJOR).$(SOEXTENSION)
endif

SHARED_LIBS = $(LIBGIFSO) $(LIBUTILSO)
STATIC_LIBS = libgif.a libutil.a

all: shared-lib static-lib $(UTILS)
ifeq ($(UNAME), Darwin)
else
	$(MAKE) -C doc
endif

$(UTILS):: $(STATIC_LIBS)

shared-lib: $(SHARED_LIBS)

static-lib: $(STATIC_LIBS)

$(LIBGIFSO): $(OBJECTS) $(HEADERS)
ifeq ($(UNAME), Darwin)
	$(CC) $(CFLAGS) -dynamiclib -current_version $(LIBVER) $(OBJECTS) -o $(LIBGIFSO)
else
	$(CC) $(CFLAGS) $(CPPFLAGS) -shared $(LDFLAGS) -Wl,-soname -Wl,$(LIBGIFSOMAJOR) -o $(LIBGIFSO) $(OBJECTS)
endif

libgif.a: $(OBJECTS) $(HEADERS)
	$(AR) rcs libgif.a $(OBJECTS)

$(LIBUTILSO): $(UOBJECTS) $(UHEADERS)
ifeq ($(UNAME), Darwin)
	$(CC) $(CFLAGS) -dynamiclib -current_version $(LIBVER) $(UOBJECTS) -o $(LIBUTILSO)
else
	$(CC) $(CFLAGS) $(CPPLAGS) -shared $(LDFLAGS) -Wl,-soname -Wl,$(LIBUTILSOMAJOR) -o $(LIBUTILSO) $(UOBJECTS)
endif

libutil.a: $(UOBJECTS) $(UHEADERS)
	$(AR) rcs libutil.a $(UOBJECTS)

clean:
	rm -f $(UTILS) $(OBSOLETE_UTILS) $(TARGET) libgetarg.a $(SHARED_LIBS) $(STATIC_LIBS) *.o
	rm -f $(LIBGIFSOVER)
	rm -f $(LIBGIFSOMAJOR)
	$(MAKE) --quiet -C doc clean

# Validate

check: all
	$(MAKE) -C tests

reflow:
	@clang-format --style="{IndentWidth: 8, UseTab: ForIndentation}" -i $$(find . -name "*.[ch]")

obsolete-utils: $(OBSOLETE_UTILS)

# cppcheck should run clean
cppcheck:
	@cppcheck --quiet --inline-suppr --template gcc --enable=all --suppress=unusedFunction --suppress=missingInclude --force *.[ch]

spellcheck:
	@spellcheck local.dic doc/*.xml

# Install/uninstall

ifeq ($(UNAME), Darwin)
install: all install-bin install-include install-lib
else
install: all install-bin install-include install-lib install-man install-doc
endif

install-bin: $(INSTALLABLE)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) $^ "$(DESTDIR)$(BINDIR)"
install-include:
	$(INSTALL) -d "$(DESTDIR)$(INCDIR)"
	$(INSTALL) -m 644 gif_lib.h "$(DESTDIR)$(INCDIR)"
install-static-lib:
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)"
	$(INSTALL) -m 644 libgif.a "$(DESTDIR)$(LIBDIR)/libgif.a"
install-shared-lib:
	$(INSTALL) -d "$(DESTDIR)$(LIBDIR)"
	$(INSTALL) -m 755 $(LIBGIFSO) "$(DESTDIR)$(LIBDIR)/$(LIBGIFSOVER)"
	ln -sf $(LIBGIFSOVER) "$(DESTDIR)$(LIBDIR)/$(LIBGIFSOMAJOR)"
	ln -sf $(LIBGIFSOMAJOR) "$(DESTDIR)$(LIBDIR)/$(LIBGIFSO)"
install-lib: install-static-lib install-shared-lib
install-man:
	$(INSTALL) -d "$(DESTDIR)$(MANDIR)/man1" "$(DESTDIR)$(MANDIR)/man7"
	$(INSTALL) -m 644 $(MANUAL_PAGES_1_MAN) "$(DESTDIR)$(MANDIR)/man1"
	$(INSTALL) -m 644 $(MANUAL_PAGES_7_MAN) "$(DESTDIR)$(MANDIR)/man7"
install-doc:
	$(MAKE) -C doc allhtml
	$(INSTALL) -d "$(DESTDIR)$(DOCDIR)/html"
	cd doc && $(INSTALL) -m 644 $$(echo *.html) "$(DESTDIR)$(DOCDIR)/html"
	@if [ -f doc/giflib-logo.gif ]; then \
		$(INSTALL) -m 644 doc/giflib-logo.gif "$(DESTDIR)$(DOCDIR)/html"; \
	fi
uninstall: uninstall-man uninstall-include uninstall-lib uninstall-bin
uninstall-bin:
	cd "$(DESTDIR)$(BINDIR)" && rm -f $(INSTALLABLE)
uninstall-include:
	rm -f "$(DESTDIR)$(INCDIR)/gif_lib.h"
uninstall-lib:
	cd "$(DESTDIR)$(LIBDIR)" && \
		rm -f libgif.a $(LIBGIFSO) $(LIBGIFSOMAJOR) $(LIBGIFSOVER)
uninstall-man:
	cd "$(DESTDIR)$(MANDIR)/man1" && rm -f $(shell cd doc >/dev/null && echo *.1)
	cd "$(DESTDIR)$(MANDIR)/man7" && rm -f $(shell cd doc >/dev/null && echo *.7)
uninstall-doc:
	rm -rf "$(DESTDIR)$(DOCDIR)/html"

# Export
#
# We include all of the XML, and also generated manual pages
# so people working from the distribution tarball won't need xmlto.

# Check that getversion hasn't gone pear-shaped.
version:
	@echo $(VERSION)

EXTRAS =     NEWS \
	     TODO \
	     COPYING \
	     getversion \
	     ChangeLog \
	     control \
	     local.dic

ALL =  Makefile *.[ch] *.adoc doc tests pic $(EXTRAS)

giflib-$(VERSION).tar.gz: $(ALL)
	mkdir giflib-$(VERSION)
	cp -r $(ALL) giflib-$(VERSION)
	tar -czf giflib-$(VERSION).tar.gz giflib-$(VERSION)
	rm -fr giflib-$(VERSION)
	ls -l giflib-$(VERSION).tar.gz

dist: giflib-$(VERSION).tar.gz

# release using the shipper tool
release: all giflib-$(VERSION).tar.gz
	$(MAKE) -C doc website
	shipper --no-stale version=$(VERSION) | sh -e -x
	rm -fr doc/staging

# Refresh the website
refresh: all
	$(MAKE) -C doc website
	shipper --no-stale -w version=$(VERSION) | sh -e -x
	rm -fr doc/staging

# end
