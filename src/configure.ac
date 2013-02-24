#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([tcLex],[1.2a1],[frederic.bonnet@ciril.fr])
AC_CONFIG_SRCDIR([tcLex.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_SANITY_CHECK
AC_ISC_POSIX
AM_SET_LEADING_DOT
AC_EXEEXT

# AX_* macros require the autoconf-archive package
AX_PROG_TCL([8.5])
AX_CHECK_GNU_MAKE

# MP_* and SC_* macros are taken from MacPorts's m4 macros
SC_PATH_TCLCONFIG
SC_LOAD_TCLCONFIG
SC_ENABLE_FRAMEWORK
SC_MISSING_POSIX_HEADERS
SC_TCL_LINK_LIBS
MP_TCL_PACKAGE
MP_TCL_THREAD_SUPPORT

if test "${prefix}" = "NONE"; then
    prefix=`echo 'puts [[cd [info library]/../..; pwd]];exit' | tclsh`
    if test "${prefix}" = ""; then
        prefix=/usr/local
        echo "no tclsh found - defaulting \"prefix\" to ${prefix}"
    else
        echo "setting prefix to: ${prefix}"
    fi
fi
if test "${exec_prefix}" = "NONE"; then
    exec_prefix=$prefix
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_LEX

# Checks for libraries.
# FIXME: Replace `main' with a function in `-ltcl':
AC_CHECK_LIB([tcl], [main])

file=${exec_prefix}/lib/tclConfig.sh
. $file

CC=$TCL_CC

# Check for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h limits.h netdb.h netinet/in.h stddef.h stdio.h sys/file.h sys/socket.h sys/time.h tclInt.h Tcl/tclInt.h Tcl/tcl-private/tclInt.h tclPort.h Tcl/tclPort.h Tcl/tcl-private/tclPort.h tclUnixPort.h Tcl/tclUnixPort.h Tcl/tcl-private/tclUnixPort.h tclIntDecls.h Tcl/tclIntDecls.h Tcl/tcl-private/tclIntDecls.h tclIntPlatDecls.h Tcl/tclIntPlatDecls.h Tcl/tcl-private/tclIntPlatDecls.h tclRegexp.h utime.h values.h])
if test "x$HAVE_TCLINT_H" = "x"; then
	AC_MSG_RESULT([tclInt.h not found, compilation will fail without it])
fi
if test "x$HAVE_TCLREGEXP_H" = "x"; then
	AC_MSG_RESULT([tclRegexp.h not found, compilation will fail without it])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday])

AC_SUBST(CC)
AC_SUBST(INSTALL)
AC_SUBST(INSTALL_PROGRAM)
AC_SUBST(INSTALL_DATA)
AC_SUBST(TCL_SHLIB_CFLAGS)
AC_SUBST(TCL_SHLIB_SUFFIX)
AC_SUBST(TCL_SHLIB_LD)
AC_SUBST(TCL_PREFIX)
AC_SUBST(TCL_EXEC_PREFIX)
AC_SUBST(TCL_CFLAGS)
AC_SUBST(TCL_DEFS)
AC_SUBST(TCL_SRC_DIR)

AC_CONFIG_FILES([Makefile Makefile.orig])
AC_OUTPUT