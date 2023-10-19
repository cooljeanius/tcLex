dnl#                                               -*- Autoconf -*-
dnl# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([tcLex],[1.2a1],[frederic.bonnet@ciril.fr])
AC_CONFIG_SRCDIR([tcLex.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])dnl

AC_CANONICAL_HOST
AC_CANONICAL_TARGET
AC_USE_SYSTEM_EXTENSIONS
AM_INIT_AUTOMAKE([1.13])
AM_MAINTAINER_MODE
AM_SANITY_CHECK
AC_SEARCH_LIBS([strerror],[cposix]) dnl# used to be AC\_ISC\_POSIX
AM_SET_LEADING_DOT
AC_EXEEXT

# AX_* macros require the autoconf-archive package
AX_PROG_TCL([8.4])
AX_CHECK_GNU_MAKE

# MP_* and SC_* macros are taken from MacPorts's m4 macros
SC_PATH_TCLCONFIG
SC_LOAD_TCLCONFIG
SC_ENABLE_FRAMEWORK
SC_MISSING_POSIX_HEADERS
SC_TCL_LINK_LIBS
MP_TCL_PACKAGE
MP_TCL_THREAD_SUPPORT

if test "x${prefix}" = "xNONE"; then
    prefix=`echo 'puts [[cd [info library]/../..; pwd]];exit' | tclsh`
    if test "x${prefix}" = "x"; then
        AC_PREFIX_DEFAULT([/usr/local])
        prefix=/usr/local
        echo "no tclsh found - defaulting \"prefix\" to ${prefix}"
    else
        echo "setting prefix to: ${prefix}"
    fi
fi
if test "x${exec_prefix}" = "xNONE"; then
    exec_prefix=${prefix}
fi

# Checks for programs.
AC_DEFUN([AC_REQUIRE_COMPILER],[
  AC_REQUIRE([AM_PROG_AS])dnl
  AC_REQUIRE([AC_PROG_CC])dnl
  AC_REQUIRE([AC_PROG_CC_C_O])dnl
  AC_REQUIRE([AC_PROG_INSTALL])dnl
  AC_REQUIRE([AC_PROG_MAKE_SET])dnl
])dnl
AC_REQUIRE_COMPILER
AM_PROG_LEX
AC_PROG_YACC

# Checks for libraries.
# FIXME: Replace `main' with a function in `-ltcl':
AC_CHECK_LIB([tcl],[main])dnl

file=${exec_prefix}/lib/tclConfig.sh
if test ! -e ${file}; then
  AC_MSG_WARN([${file} not found; trying to find a different tclConfig.sh...])
  ## TODO: might want to use some sort of looping here to try all possibilities,
  ## and then choose the best one:
  if test "x${prefix}" != "x${exec_prefix}"; then
    file=${prefix}/lib/tclConfig.sh
  elif test -x "`which locate`"; then
    AC_MSG_NOTICE([attempting to use locate to find tclConfig.sh])
    file="`locate /tclConfig.sh | head -n 1`";
  elif test -x "`which glocate`"; then
    AC_MSG_NOTICE([attempting to use glocate to find tclConfig.sh])
    file="`glocate /tclConfig.sh | head -n 1`";
  elif test -x "`which mlocate`"; then
    AC_MSG_NOTICE([attempting to use mlocate to find tclConfig.sh])
    file="`mlocate /tclConfig.sh | head -n 1`";
  elif test -x "`which plocate`"; then
    AC_MSG_NOTICE([attempting to use plocate to find tclConfig.sh])
    file="`plocate /tclConfig.sh | head -n 1`";
  elif test -x "`which mdfind`"; then
    AC_MSG_NOTICE([attempting to use mdfind to find tclConfig.sh])
    file="`mdfind -name /tclConfig.sh | head -n 1`";
  elif test -x "`which find`"; then
    AC_MSG_WARN([attempting to use find to find tclConfig.sh; this may take awhile...])
    if test -d "${exec_prefix}"; then
      file="`find ${exec_prefix} -name /tclConfig.sh -print 2>/dev/null | head -n 1`";
    elif test -d "${prefix}"; then
      file="`find ${prefix} -name /tclConfig.sh -print 2>/dev/null | head -n 1`";
    else
      file="`find / -name /tclConfig.sh -print 2>/dev/null | head -n 1`";
    fi
  else
    file=/dev/null
  fi
fi
AC_MSG_NOTICE([sourcing ${file}])
. ${file}

CC=${TCL_CC}

# Check for header files.
if test "x0" = "Y1" && test -x "`which autoscan`"; then
  dnl# keep autoscan happy
  AC_CHECK_HEADERS([float.h inttypes.h limits.h net/errno.h stdint.h sys/param.h \
                    unistd.h values.h])
  AC_HEADER_SYS_WAIT
else
  dnl# these should already be cached so only check once
  AC_CHECK_HEADERS_ONCE([float.h inttypes.h limits.h net/errno.h stdint.h \
                         sys/param.h unistd.h values.h])
  if test "x${ac_cv_header_stdc}" = "x"; then
    test -z "${ac_cv_header_stdc}"
    AC_HEADER_STDC
  fi
fi
AC_HEADER_DIRENT
AC_HEADER_TIME
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h pwd.h \
                  signal.h stdarg.h stddef.h stdio.h sys/file.h sys/socket.h \
                  sys/time.h tclInt.h Tcl/tclInt.h \
                  Tcl/tcl-private/tclInt.h tclPort.h Tcl/tclPort.h \
                  Tcl/tcl-private/tclPort.h tclUnixPort.h \
                  Tcl/tclUnixPort.h Tcl/tcl-private/tclUnixPort.h \
                  tclIntDecls.h Tcl/tclIntDecls.h \
                  Tcl/tcl-private/tclIntDecls.h tclIntPlatDecls.h \
                  Tcl/tclIntPlatDecls.h Tcl/tcl-private/tclIntPlatDecls.h \
                  tclRegexp.h tclTomMathDecls.h utime.h])
if test "x${HAVE_TCLINT_H}" = "x" -a "x${ac_cv_header_tclInt_h}" = "xno"; then
	AC_MSG_WARN([tclInt.h not found, compilation will most likely fail without it])
fi
if test "x${HAVE_TCLREGEXP_H}" = "x" -a "x${ac_cv_header_tclRegexp_h}" = "xno"; then
	AC_MSG_WARN([tclRegexp.h not found, compilation will most likely fail without it])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_PROTOTYPES
AC_TYPE_INTMAX_T
AC_TYPE_INTPTR_T
AC_TYPE_UINTPTR_T
AC_TYPE_UID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])dnl

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday])dnl

# Output.
AC_SUBST([CC])dnl
AC_SUBST([INSTALL])dnl
AC_SUBST([INSTALL_PROGRAM])dnl
AC_SUBST([INSTALL_DATA])dnl
AC_SUBST([TCL_SHLIB_CFLAGS])dnl
AC_SUBST([TCL_SHLIB_SUFFIX])dnl
AC_SUBST([TCL_SHLIB_LD])dnl
AC_SUBST([TCL_PREFIX])dnl
AC_SUBST([TCL_EXEC_PREFIX])dnl
AC_SUBST([TCL_CFLAGS])dnl
AC_SUBST([TCL_DEFS])dnl
AC_SUBST([TCL_SRC_DIR])dnl

AC_CONFIG_FILES([Makefile Makefile_orig])
AC_OUTPUT
