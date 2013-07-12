#! /bin/bash
#
# $Id$
#
# Author: Boris Jakubith
# E-Mail: bj@isv-gmbh.de
# Copyright: © 2005, Boris Jakubith <bj@isv-gmbh.de>
# Licence: GPL(v2)

PROG="${0##*/}"
fp="${0%/*}/fullpath"
top="$($fp -b 2 "$0")"
adm="$top/admin"
cur="$(pwd)"
bld="$cur/pkg-build"

error() {
    local ec="$1"; shift
    echo 1>&2 "${PROG}:" "$@"
    exit $ec
}

# Get the last version number of the 'changelog' file ...
#
if [ -d "$top/debian" ]; then
    deb_source="$top/debian"
    deb_changelog="$top/debian/changelog"
else
    deb_source="$adm/debian"
    deb_changelog="$adm/debian/changelog"
fi

test -f "$deb_changelog" || error 66 "'$deb_changelog' not found"

DV=`head -1 "$deb_changelog"|\
    sed -e 's|[^(]*(\([0-9]\+\.[0-9.]\+\)[^)]*).*|\1|'`
#
# and the current package version ...
#
PV="$(cat "$top/VERSION")"

# Check if both version numbers differ ...
#
if [ "$DV" != "$PV" ]; then
    # If so, then insert a new entry into the `debian/changelog'-directory ...
    #
    # ... generate a new debian version number ...
    DV=`head -1 "$deb_changelog"|\
	sed -e "s|.*(\([0-9]\+\.[0-9]\+\)\(.*\)).*|$PV\2|"`

    # ... generate the date for the `AUTHOR  DATE'-entry ...
    DT=$(unset LANG LC_CTYPE LC_ALL; date -R)

    package="$($adm/version packagename)"
    maintainer="$($adm/dcedit -m)"
    # ... generate the new entry ...
    cat <<-EOT >"${deb_changelog}.new"
	$package ($DV) unstable; urgency=low

	  * New upstream release.
	
	 -- $maintainer  $DT
	
	EOT
    # ... append the existing 'debian/changelog'-file ...
    cat "$deb_changelog" >>"${deb_changelog}.new"
    # ... copy back to 'debian/changelog' ...
    cat "${deb_changelog}.new" >"$deb_changelog"
    # ... remove the temporary file ...
    rm -f "${deb_changelog}.new"
fi

if [ $# -gt 0 -a "x$1" = xchangelog ]; then exit 0; fi

# Generate the directory for the 'Debian'-source ...
#
mkdir -m 0755 "$bld"

# Copy the required files into it ...
#
find . \( -name 'CVS' -o -path '*/CVS/*' \) -prune \
       -o \( -name '.svn' -o -path '*/.svn/*' \) -prune \
       -o -name 'debdir' -prune \
       -o -path 'debdir/*' -prune -o -print \
| cpio -pdm "$bld"

if false; then
# Remove the debian sub-directory in the target, because it will be replaced
# by another one ...
#

# Copy the 'debian' sub-directory from either $top or $adm to here ...
#
if [ -d "$top/debian" ]; then
    cpsrc="$top/debian"; d="$top"
else
    cpsrc="$adm/debian"; d="$adm"
fi

# Change into the "debian" source directory and copy all files there (with the
# exception of the 'CVS' and '.svn' sub-directories) to the 'debdir' sub-
# directory in the previous directory ...
#
cd "$d"
find debian \( -name CVS -o -path '*/CVS/*' \) -prune \
	    -o \( -name .svn -o -path '*/.svn/*' \) -prune \
	    -o -print \
| perl -ne 's|^\Q'"$d"'\E/|./|; print $_;' \
| cpio -pdm "$bld"

# Change back into $cur, but go a step further by changing into the sub-
# directory $bld below $cur, because this is out build-directory ...
#
cd "$bld"

# Now modify (extend) the file 'debian/changelog' below the build-directory ...
#
"$adm/dcedit"

fi # if false;

# Change into the 'debdir' sub-directory ...
cd "$bld"

# Use 'dpkg-buildpackage' for generating the debian packages, but only the
# binary packages and without signing these packages ...
#
dpkg-buildpackage -rfakeroot -b #-us -uc

# Change back into parent directory and remove the build-directory
# ('debdir') ...
#
cd "$cur"; rm -rf "$bld"

# Last not least, move the '.deb'-files outside of this directory ...
#
#mv *.deb *.changes ..

exit 0
