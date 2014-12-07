AC_INIT([tracker-miner-media],
        [0.1.0],
        [http://bugzilla.gnome.org/],
        [tracker-miner-media],
        [https://wiki.gnome.org/Projects/Tracker])
AC_PREREQ(2.63)

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_INIT_AUTOMAKE([dist-xz no-dist-gzip tar-ustar])
AM_MAINTAINER_MODE([enable])

AC_PROG_CC
AC_PROG_CC_C_O
AC_HEADER_STDC

PKG_PROG_PKG_CONFIG

LT_INIT([])

#dependencies
PKG_CHECK_MODULES(DEPS, [tracker-miner-1.0 libgdata >= 0.17])

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT
