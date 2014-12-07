#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="tracker-movie-miner"

(test -f $srcdir/src/tracker-miner-media.c) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

# if the AC_CONFIG_MACRO_DIR() macro is used, create that directory
# This is a automake bug fixed in automake 1.13.2
# See http://debbugs.gnu.org/cgi/bugreport.cgi?bug=13514
if [ -n "m4" ]; then
        mkdir -p m4
fi

intltoolize --force --copy --automake || exit 1
autoreconf --verbose --force --install || exit 1

$srcdir/configure "$@" && echo "Now type \"make\" to compile $PKG_NAME" || exit 1
