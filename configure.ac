dnl#                                               -*- Autoconf -*-
dnl# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([tcLex],[1.2a1],[frederic.bonnet@ciril.fr])
AC_CONFIG_SRCDIR([examples/pascal/pascal.l])
AC_CONFIG_HEADERS([config.h])dnl

# Checks for programs.
AC_PROG_CC
AM_INIT_AUTOMAKE([1.13 foreign])dnl

# Check for header files.
AC_CHECK_HEADERS([net/errno.h msvcrt/errno.h])dnl

# Output.
AC_CONFIG_FILES([Makefile])
AC_CONFIG_SUBDIRS([src])
AC_OUTPUT
