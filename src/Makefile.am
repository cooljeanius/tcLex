#
# This makefile builds tcLex, a lexer generator, as a dynamically
# loadable Tcl extension.  This makefile is suitable for use on
# Unix systems.
#

ACLOCAL_AMFLAGS = -I m4 --install

#
# Project setting -- version and stuff
#
# PROJECT -- name of the project. Used in the file and package names
# PROJECT_VERSION -- version of the project, in major.minor format
# VERSION_DEFINE -- symbol used in code that hold the version number

PROJECT		= tcLex
PROJECT_VERSION	= 1.2
VERSION_DEFINE	= TCLEX_VERSION

# Object files
OBJS		= tcLex.o \
		  tcLexRE.o

tcl_MANS_COMMENTED_OUT = tclex.n

#-------------------------------------------------
INSTALL         = @INSTALL@
INSTALL_PROGRAM = @INSTALL_PROGRAM@
INSTALLdir      = @INSTALLdir@
INSTALL_DATA    = @INSTALL_DATA@
CC              = @CC@
SHLIB_CFLAGS    = @TCL_SHLIB_CFLAGS@
SHLIB_SUFFIX    = @TCL_SHLIB_SUFFIX@
SHLIB_LD        = @TCL_SHLIB_LD@
TCL_PREFIX      = @TCL_PREFIX@
TCL_CFLAGS      = @TCL_CFLAGS@
TCL_SRC_DIR	= @TCL_SRC_DIR@

AM_CPPFLAGS     = -I$(TCL_SRC_DIR)/generic -I$(TCL_PREFIX)/include
AM_WARNFLAGS    = -Wall

DLL             = $(PROJECT)$(SHLIB_SUFFIX)
PROJECTDIR	= $(TCL_PREFIX)/lib/$(PROJECT)$(PROJECT_VERSION)

CFLAGS          = $(TCL_CFLAGS) $(SHLIB_CFLAGS) $(INCLUDES) \
		  -D$(VERSION_DEFINE)=\"$(PROJECT_VERSION)\" \
                  -DHAVE_CONFIG_H

MAN_INSTALL_DIR = $(TCL_PREFIX)/man/mann

all: $(DLL) pkgIndex.tcl

$(DLL): $(OBJS)
	$(SHLIB_LD) -o $@ $(OBJS)

tcLex.o: tcLex.c tcLex.h tcLexInt.h tcLexRE.h tclInt.h
	$(CC) $(CFLAGS) -c -o tcLex.o tcLex.c

tcLexRE.o: tcLexRE.c tcLex.h tcLexRE.h tclInt.h RE80.c RE81.c RE82.c
	$(CC) $(CFLAGS) $(AM_CPPFLAGS) -c -o tcLexRE.o tcLexRE.c

pkgIndex.tcl:
	(if test -f $(PROJECTDIR)/pkgIndex.tcl; \
	    then grep -v $(PROJECT_VERSION) $(PROJECTDIR)/pkgIndex.tcl; \
	else true; fi;) | \
	echo 'package ifneeded $(PROJECT) $(PROJECT_VERSION) [list load [file join $$dir $(DLL).$(PROJECT_VERSION)] $(PROJECT)]' > pkgIndex.tcl

install_tclex: all
	if test ! -d $(PROJECTDIR); then mkdir $(PROJECTDIR); fi
	$(INSTALL_PROGRAM) $(DLL) $(PROJECTDIR)/$(DLL).$(PROJECT_VERSION)
	$(INSTALL_DATA) pkgIndex.tcl $(PROJECTDIR)/pkgIndex.tcl

test: all
	true

clean-local: 
	rm -f *.o core pkgIndex.tcl

distclean-local: clean-local
	rm -f Makefile config.status $(DLL)

DISTCLEANFILES = Makefile.orig

# Hack: these should not be treated as targets in the first place:
.PHONY: -c -m 644
