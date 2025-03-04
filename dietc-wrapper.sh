#! /bin/sh
# cc
#
# $Id$
#
# Author: Boris Jakubith
# E-Mail: runkharr@googlemail.com
# Copyright: (c) 2025, Boris Jakubith <runkharr@googlemail.com>
# License: GNU General Public License, version 2
#
# Small wrapper which invokes `diet <compiler>` if
# the environment variable `DIETC` is set ant not empty
# and also if `diet` is available

PROG="$(basename "$0")"
BASE="${PROG%.sh}"

diet=
if [ -n "$DIETC" ]; then
    diet="$(command -v diet)" || true
fi

exec ${diet:+"$diet"} "$BASE" ${1+"$@"}
